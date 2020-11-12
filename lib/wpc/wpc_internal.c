/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/**
 * This file is the core part of the C Mesh application
 */
#define LOG_MODULE_NAME "wpc_int"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#define PRINT_BUFFERS
#include "logger.h"

#include "serial.h"
#include "slip.h"
#include "wpc_types.h"
#include "wpc_internal.h"
#include "util.h"
#include "platform.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

// This is the timeout in milliseconds to receive an indication
// from stack after a poll request
#define TIMEOUT_INDICATION_MS 500

// This is the default timeout in millisecond to receive a confirm from
// the stack for a request
#define TIMEOUT_CONFIRM_MS 500

// Attempt to receive confirm from a request.
// In some cases like OTAP it can happen that Dual MCU app doesn't answer
// for a long time (can be up to 1mins when exchanging an OTAP with a neighbor).
// So all the poll requests done during this period can be received in a raw
// So this mechanism allows to flush previous poll requests. Try several
// times to get the right confirm.
#define MAX_CONFIRM_ATTEMPT 50

// Struct that describes a received frame with its timestamp
typedef struct
{
    wpc_frame_t frame;                      //< The received frame
    unsigned long long timestamp_ms_epoch;  //< The timestamp of reception
} timestamped_frame_t;

/*****************************************************************************/
/*                Response implementation                                    */
/*****************************************************************************/
/**
 * \brief   Send a response to stack to acknowledge an indication
 * \param   primitive_id
 *          id of the response
 * \param   frame_id
 *          id of the corresponding indication frame_id
 * \param   more_ind
 *          true if more indications must be sent
 * \return  0 if success, negative value otherwise
 */
static int send_response_to_stack(uint8_t primitive_id, uint8_t frame_id, bool more_ind)
{
    wpc_frame_t frame;

    frame.primitive_id = primitive_id;
    frame.frame_id = frame_id;
    frame.payload_length = 1;
    frame.payload.sap_response_payload.result = more_ind ? 1 : 0;

    return Slip_send_buffer((uint8_t *) &frame, 4);
}

/*****************************************************************************/
/*                Request implementation                                     */
/*****************************************************************************/

/**
 * \brief   This function send a request to the stack and wait for confirmation
 * \param   request
 *          The request to send
 * \param   confirm
 *          The confirm received by the stack
 * \param   timeout_ms
 *          Timeout to wait for confirm after request in ms
 * \return  0 if success, a negative value otherwise
 *
 * \note    This function MUST be called with sending_mutex locked
 */
static int send_request_locked(wpc_frame_t * request, wpc_frame_t * confirm, uint16_t timeout_ms)
{
    static uint8_t frame_id = 0;
    int confirm_size;
    int attempt = 0;
    uint8_t buffer[MAX_FRAME_SIZE];
    wpc_frame_t * rec_confirm;

    LOGD("Send_request LOCK \n");

    // fill the frame id request
    request->frame_id = frame_id++;

    // Send the request
    if (Slip_send_buffer((uint8_t *) request, request->payload_length + 3) < 0)
    {
        LOGE("Cannot send request\n");
        return WPC_INT_GEN_ERROR;
    }

    while (attempt < MAX_CONFIRM_ATTEMPT)
    {
        // Wait for confirm during TIMEOUT_CONFIRM seconds
        confirm_size = Slip_get_buffer(buffer, sizeof(buffer), timeout_ms);
        if (confirm_size < 0)
        {
            LOGE("Didn't receive answer to the request 0x%02x\n", request->primitive_id);
            // Return confirm_size to propagate the error code
            return confirm_size;
        }

        // Even if it doesn't match, the node sent something so it is alive
        Platform_valid_message_from_node();

        // Check the confirm
        rec_confirm = (wpc_frame_t *) buffer;
        if ((rec_confirm->primitive_id) != (request->primitive_id + SAP_CONFIRM_OFFSET))
        {
            LOGW("Waiting confirm for primitive_id 0x%02x but received "
                 "0x%02x\n",
                 request->primitive_id + SAP_CONFIRM_OFFSET,
                 rec_confirm->primitive_id);
            // LOG_PRINT_BUFFER(buffer, FRAME_SIZE((wpc_frame_t *) buffer));
            attempt++;
            continue;
        }
        else if (rec_confirm->frame_id != request->frame_id)
        {
            LOGW("Waiting confirm for frame_id 0x%02x but received 0x%02x\n",
                 request->frame_id,
                 rec_confirm->frame_id);
            // LOG_PRINT_BUFFER(buffer, FRAME_SIZE((wpc_frame_t *) buffer));
            attempt++;
            continue;
        }
        break;
    }

    if (attempt == MAX_CONFIRM_ATTEMPT)
    {
        LOGE("Synchronization lost\n");
        return WPC_INT_SYNC_ERROR;
    }

    // Copy the confirm
    memcpy((uint8_t *) confirm, buffer, confirm_size);
    return 0;
}

/*****************************************************************************/
/*                Indication implementation                                  */
/*****************************************************************************/
void WPC_Int_dispatch_indication(wpc_frame_t * frame, unsigned long long timestamp_ms)
{
    switch (frame->primitive_id)
    {
        case DSAP_DATA_TX_INDICATION:
            dsap_data_tx_indication_handler(&frame->payload.dsap_data_tx_indication_payload);
            break;
        case DSAP_DATA_RX_INDICATION:
            dsap_data_rx_indication_handler(&frame->payload.dsap_data_rx_indication_payload,
                                            timestamp_ms);
            break;
        case MSAP_STACK_STATE_INDICATION:
            msap_stack_state_indication_handler(&frame->payload.msap_stack_state_indication_payload);
            break;
        case MSAP_APP_CONFIG_DATA_RX_INDICATION:
            msap_app_config_data_rx_indication_handler(
                &frame->payload.msap_app_config_data_rx_indication_payload);
            break;
        case MSAP_SCRATCH_REMOTE_STATUS_INDICATION:
            msap_image_remote_status_indication_handler(
                &frame->payload.msap_image_remote_status_indication_payload);
            break;
        case MSAP_SCAN_NBORS_INDICATION:
            msap_scan_nbors_indication_handler(&frame->payload.msap_scan_nbors_indication_payload);
            break;
        default:
            LOGE("Unknown indication 0x%02x\n", frame->primitive_id);
            LOG_PRINT_BUFFER((uint8_t *) frame, FRAME_SIZE(frame));
    }
}

/**
 * \brief   Handler for poll confirm
 * \param   last_one
 *          Is it the last indication to read
 * \param   indication_p
 *          Pointer to store the received indication
 * \return  negative value in case of error, 0 if no more indication,
 *          1 if indication still pending
 */
static int handle_indication(bool last_one, onIndicationReceivedLocked_cb_f cb_locked)
{
    int res;
    int remaining_ind;
    wpc_frame_t frame;
    unsigned long long timestamp_ms_epoch;

    LOGD("Pending indication from stack, wait for it\n");
    res = Slip_get_buffer((uint8_t *) &frame, sizeof(wpc_frame_t), TIMEOUT_INDICATION_MS);
    if (res <= 0)
    {
        LOGE("Timeout waiting for indication last_one=%d\n", last_one);
        return WPC_INT_TIMEOUT_ERROR;
    }

    // Get timestamp just after reception
    timestamp_ms_epoch = Platform_get_timestamp_ms_epoch();

    remaining_ind = frame.payload.generic_indication_payload.indication_status;

    // Answer the stack
    send_response_to_stack(frame.primitive_id + SAP_RESPONSE_OFFSET,
                           frame.frame_id,
                           (remaining_ind > 0) && !last_one);

    // Call the cb to handle it (implemented by platform)
    cb_locked(&frame, timestamp_ms_epoch);

    return remaining_ind;
}

/**
 * \brief   Retrieve indications from the sink
 * \param   indications
 *          Table to store the retrieved indications
 * \param   max_ind
 *          Maximum number of indications to read
 * \param   read_ind_p
 *          Pointer to store the number of read indications
 * \return  Negative value in case of error
 *          0 if no more indication pending
 *          1 if at least 1 indications is still pending
 */
static int get_indication_locked(unsigned int max_ind, onIndicationReceivedLocked_cb_f cb_locked)
{
    wpc_frame_t request;
    wpc_frame_t confirm;
    int remaining_ind = 1;
    int ret;

    if (max_ind == 0)
    {
        // Poll request cannot be done if no indication
        // can be handled
        LOGE("Wrong number of indication\n");
        return WPC_INT_WRONG_PARAM_ERROR;
    }

    request.primitive_id = MSAP_INDICATION_POLL_REQUEST;
    request.payload_length = 0;

    LOGD("Start a poll request\n");
    ret = send_request_locked(&request, &confirm, TIMEOUT_CONFIRM_MS);
    if (ret < 0)
    {
        LOGE("Unable to poll for request\n");
        return ret;
    }

    if (confirm.payload.sap_generic_confirm_payload.result == 0)
    {
        // There is no pending indication
        return 0;
    }

    remaining_ind = 1;
    while (max_ind-- && remaining_ind)
    {
        bool last_one = (max_ind == 0);
        remaining_ind = handle_indication(last_one, cb_locked);
        if (remaining_ind < 0)
        {
            LOGE("Cannot get an indication\n");
            return remaining_ind;
        }
    }

    return remaining_ind > 0 ? 1 : 0;
}

int WPC_Int_get_indication(unsigned int max_ind, onIndicationReceivedLocked_cb_f cb_locked)
{
    Platform_lock_request();
    int res = get_indication_locked(max_ind, cb_locked);
    Platform_unlock_request();

    return res;
}

int WPC_Int_send_request_timeout(wpc_frame_t * frame, wpc_frame_t * confirm, uint16_t timeout_ms)
{
    Platform_lock_request();
    int res = send_request_locked(frame, confirm, timeout_ms);
    Platform_unlock_request();

    return res;
}

int WPC_Int_send_request(wpc_frame_t * frame, wpc_frame_t * confirm)
{
    // Timeout not specified so use default one
    return WPC_Int_send_request_timeout(frame, confirm, TIMEOUT_CONFIRM_MS);
}

int WPC_Int_initialize(char * port_name, unsigned long bitrate)
{
    // Open the serial connection
    if (Serial_open(port_name, bitrate) < 0)
        return WPC_INT_GEN_ERROR;

    // Initialize the slip module
    Slip_init(&Serial_write, &Serial_read);

    if (!Platform_init())
    {
        Serial_close();
        return WPC_INT_GEN_ERROR;
    }

    dsap_init();

    LOGI("WPC initialized\n");
    return 0;
}

void WPC_Int_close(void)
{
    Platform_close();

    Serial_close();
}
