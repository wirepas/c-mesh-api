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
#include <stdlib.h>

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

// Maximum attempt to retrieve request in case a CRC error was detetcted in
// the request. It is detected when node sends a dummy confirm with CRC
// explicitly set to 0xFFFF
#define MAX_CRC_REQUEST_ERROR_RETRIES 3

// Maximum duration in s of failed poll request to declare the link broken
// (unplugged) Set it to 0 to disable 60 sec is long period but it must cover
// the OTAP exchange with neighbors that can be long with some profiles
#define DEFAULT_MAX_POLL_FAIL_DURATION_MS (60 * 1000)

// Last successful exchange with node
static unsigned long long m_last_successful_answer_ts;

// Max delay before exiting
static unsigned int m_timeout_no_answer_ms = DEFAULT_MAX_POLL_FAIL_DURATION_MS;

// Maximum transmission unit for a single PDU without segmentation
static unsigned int m_mtu = DEFAULT_MTU_SIZE;

// Struct that describes a received frame with its timestamp
typedef struct
{
    wpc_frame_t frame;                      //< The received frame
    unsigned long long timestamp_ms_epoch;  //< The timestamp of reception
} timestamped_frame_t;

static bool m_disabled_poll_request = false;

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

static bool check_if_timeout_reached_locked()
{
    // Check if it has failed for too long
    if (m_timeout_no_answer_ms > 0)
    {
        if ((Platform_get_timestamp_ms_monotonic() - m_last_successful_answer_ts) > m_timeout_no_answer_ms)
        {
            // Poll request has failed for too long
            // This is a fatal error as the com with sink was not possible
            // for a too long period.
            // Close the platform but release our lock first as someone else could be waiting for it
            // No need to keep, we are exiting anyway
            Platform_unlock_request();

            Platform_close();
            exit(EXIT_FAILURE);

            // Never executed
            return true;
        }
    }
    return false;
}
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
    uint8_t crc_request_retries = 0;
    uint8_t buffer[MAX_FRAME_SIZE];
    wpc_frame_t * rec_confirm;

    LOGD("Send_request LOCK \n");

    // fill the frame id request
    request->frame_id = frame_id++;

    // Send the request
    if (Slip_send_buffer((uint8_t *) request, request->payload_length + 3) < 0)
    {
        check_if_timeout_reached_locked();
        return WPC_INT_GEN_ERROR;
    }

    while (attempt < MAX_CONFIRM_ATTEMPT)
    {
        // Wait for confirm during TIMEOUT_CONFIRM seconds
        confirm_size = Slip_get_buffer(buffer, sizeof(buffer), timeout_ms);
        if (confirm_size < 0)
        {
            if (confirm_size == WPC_INT_WRONG_CRC_FROM_HOST)
            {
                // CRC error is in the request, so not handled on the other side
                // Safe to resend it
                if (crc_request_retries++ < MAX_CRC_REQUEST_ERROR_RETRIES)
                {
                    LOGW("Wrong CRC for request 0x%02x, send it again %d/%d\n", request->primitive_id, crc_request_retries, MAX_CRC_REQUEST_ERROR_RETRIES);
                    if (Slip_send_buffer((uint8_t *) request, request->payload_length + 3) < 0)
                    {
                        check_if_timeout_reached_locked();
                        return WPC_INT_GEN_ERROR;
                    }
                    continue;
                }

                LOGE("Too many CRC errors for request (%d), propagate error\n", MAX_CRC_REQUEST_ERROR_RETRIES);
            }
            else if (confirm_size == WPC_INT_WRONG_CRC)
            {
                // We received a CRC error for the confirm,
                // we cannot resend the request as we don't know if it was executed or not
                LOGE("CRC error in confirm for request 0x%02x\n", request->primitive_id);
            }
            else
            {
                LOGE("Didn't receive answer to the request 0x%02x error is: %d\n", request->primitive_id, confirm_size);
                check_if_timeout_reached_locked();
            }

            // Return confirm_size to propagate the error code
            return confirm_size;
        }

        // Even if it doesn't match, the node sent something so it is alive
        // Update our last activity
        m_last_successful_answer_ts = Platform_get_timestamp_ms_monotonic();

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
static void dispatch_indication(wpc_frame_t * frame, unsigned long long timestamp_ms)
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
        case DSAP_DATA_RX_FRAG_INDICATION:
            dsap_data_rx_frag_indication_handler(&frame->payload.dsap_data_rx_frag_indication_payload,
                                            timestamp_ms);
            break;
        case MSAP_STACK_STATE_INDICATION:
            msap_stack_state_indication_handler(&frame->payload.msap_stack_state_indication_payload);
            break;
        case MSAP_APP_CONFIG_DATA_RX_INDICATION:
            msap_app_config_data_rx_indication_handler(
                &frame->payload.msap_app_config_data_rx_indication_payload);
            break;
        case MSAP_SCAN_NBORS_INDICATION:
            msap_scan_nbors_indication_handler(&frame->payload.msap_scan_nbors_indication_payload);
            break;
        case MSAP_CONFIG_DATA_ITEM_RX_INDICATION:
            msap_config_data_item_rx_indication_handler(&frame->payload.msap_config_data_item_rx_indication_payload);
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
        LOGE("No poll answer\n");
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

static int get_indication(unsigned int max_ind, onIndicationReceivedLocked_cb_f cb_locked)
{
    int res;
    Platform_lock_request();
    if (!m_disabled_poll_request)
    {
        res = get_indication_locked(max_ind, cb_locked);
    }
    else
    {
        // Poll is temporarly disabled, just pretend there is nothing
        res = 0;
    }
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

void WPC_Int_disable_poll_request(bool disabled)
{
    m_disabled_poll_request = disabled;
}

int WPC_Int_send_request(wpc_frame_t * frame, wpc_frame_t * confirm)
{
    // Timeout not specified so use default one
    return WPC_Int_send_request_timeout(frame, confirm, TIMEOUT_CONFIRM_MS);
}

bool WPC_Int_set_timeout_s_no_answer(unsigned int duration_s)
{
    if ((m_timeout_no_answer_ms == 0) && (duration_s > 0))
    {
        // Initialize last activity to now (if it is first not enabled yet)
        m_last_successful_answer_ts = Platform_get_timestamp_ms_monotonic();
    }
    m_timeout_no_answer_ms = duration_s * 1000;
    return true;
}

int WPC_Int_initialize(const char * port_name, unsigned long bitrate)
{
    // Open the serial connection
    if (Serial_open(port_name, bitrate) < 0)
        return WPC_INT_GEN_ERROR;

    // Initialize the slip module
    Slip_init(&Serial_write, &Serial_read);

    if (!Platform_init(get_indication, dispatch_indication))
    {
        Serial_close();
        return WPC_INT_GEN_ERROR;
    }

    dsap_init();

    m_last_successful_answer_ts = Platform_get_timestamp_ms_monotonic();

    LOGI("WPC initialized\n");
    return 0;
}

void WPC_Int_close(void)
{
    Platform_close();

    Serial_close();
}

void WPC_Int_set_mtu(void)
{
    uint8_t mtu = DEFAULT_MTU_SIZE;
    if (WPC_get_mtu(&mtu) == APP_RES_OK)
    {
        m_mtu = mtu;
    }
}

uint8_t WPC_Int_get_mtu(void)
{
    return m_mtu;
}
