/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "msap"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "wpc_types.h"
#include "wpc_internal.h"
#include "util.h"
#include "platform.h"

#include "string.h"

/**
 * \brief   Registered callback for app config
 */
static onAppConfigDataReceived_cb_f m_app_conf_cb = NULL;

/**
 * \brief   Registered callback for remote status
 */
static onRemoteStatus_cb_f m_remote_status_cb = NULL;

/**
 * \brief   Registered callback for scan neighbors
 */
static onScanNeighborsDone_cb_f m_scan_neighbor_cb = NULL;

/**
 * \brief   Registered callback for stack status neighbors
 */
static onStackStatusReceived_cb_f m_stack_status_cb = NULL;

/**
 * \brief   Time to wait for scratchpad start request confirm
 *          It can be long because the start triggers an erase of
 *          scratchpad area that can be long if external memory used
 */
#define SCRATCHPAD_START_TIMEOUT_MS 45000

/**
 * \brief   Time to wait for scratchpad block request confirm
 *          It can be long because writing flash can be long
 *          especially if written in external memory
 */
#define SCRATCHPAD_BLOK_TIMEOUT_MS 5000

int msap_stack_start_request(uint8_t start_option)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_STACK_START_REQUEST;

    request.payload.msap_stack_start_request_payload.start_option = start_option;

    request.payload_length = sizeof(msap_stack_start_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGI("Start request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_stack_stop_request()
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_STACK_STOP_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);
    if (res < 0)
        return res;

    LOGI("Stop request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_app_config_data_write_request(uint8_t seq, uint16_t interval, uint8_t * config_p, uint8_t size)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_APP_CONFIG_DATA_WRITE_REQUEST;
    request.payload.msap_app_config_data_write_request_payload.sequence_number = seq;
    request.payload.msap_app_config_data_write_request_payload.diag_data_interval = interval;
    // Initialize config data to 0
    memset(request.payload.msap_app_config_data_write_request_payload.app_config_data,
           0,
           MAXIMUM_APP_CONFIG_SIZE);
    memcpy(request.payload.msap_app_config_data_write_request_payload.app_config_data, config_p, size);

    request.payload_length = sizeof(msap_app_config_data_write_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGI("App config write result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_app_config_data_read_request(uint8_t * seq, uint16_t * interval, uint8_t * config_p, uint8_t size)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_APP_CONFIG_DATA_READ_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    if (confirm.payload.sap_generic_confirm_payload.result == 0)
    {
        *seq = confirm.payload.msap_app_config_data_read_confirm_payload.sequence_number;
        *interval = confirm.payload.msap_app_config_data_read_confirm_payload.diag_data_interval;
        memcpy(config_p,
               confirm.payload.msap_app_config_data_read_confirm_payload.app_config_data,
               size);

        LOGI("App config : seq=%d, interval=%d\n", *seq, *interval);
    }
    else
    {
        LOGI("No App config Set\n");
    }

    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_sink_cost_write_request(uint8_t cost)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SINK_COST_WRITE_REQUEST;
    request.payload.msap_sink_cost_write_request_payload.cost = cost;
    request.payload_length = sizeof(msap_sink_cost_write_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGD(" Cost write request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_sink_cost_read_request(uint8_t * cost_p)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SINK_COST_READ_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    if (confirm.payload.msap_sink_cost_read_confirm_payload.result == 0)
    {
        *cost_p = confirm.payload.msap_sink_cost_read_confirm_payload.cost;
        LOGD("Sink Cost Read : %d\n", *cost_p);
    }
    else
    {
        LOGW("Try to read cost sink, but node is not a sink\n");
    }

    return confirm.payload.msap_sink_cost_read_confirm_payload.result;
}

int msap_get_nbors_request(msap_get_nbors_conf_pl_t * neigbors_p)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_GET_NBORS_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);
    if (res < 0)
        return res;

    LOGI("Get nbors request result. Found %d neighbors\n",
         confirm.payload.msap_get_nbors_confirm_payload.number_of_neighbors);
    memcpy((uint8_t *) neigbors_p,
           (uint8_t *) &confirm.payload.msap_get_nbors_confirm_payload,
           sizeof(msap_get_nbors_conf_pl_t));
    return APP_RES_OK;
}

int msap_scan_nbors_request()
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCAN_NBORS_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);
    if (res < 0)
        return res;

    LOGI("Scan nbors request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_start_request(uint32_t length, uint8_t seq)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_START_REQUEST;
    request.payload.msap_image_start_request_payload.scratchpad_length = length;
    request.payload.msap_image_start_request_payload.scratchpad_sequence_number = seq;
    request.payload_length = sizeof(msap_image_start_req_pl_t);

    // Starting a scrtachpad may trigger an erase of scratchpad area so can be
    // quite long operation and confirm can be delayed for quite a long time.
    // So set timeout to higher value
    res = WPC_Int_send_request_timeout(&request, &confirm, SCRATCHPAD_START_TIMEOUT_MS);

    if (res < 0)
    {
        LOGE("Start scratchpad request result = %d timeout is %d\n", res, SCRATCHPAD_START_TIMEOUT_MS);
        return res;
    }

    LOGI("Start scratchpad request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_block_request(uint32_t start_address, uint8_t number_of_bytes, uint8_t * bytes)
{
    wpc_frame_t request, confirm;
    int res;

    if (number_of_bytes > MAXIMUM_SCRATCHPAD_BLOCK_SIZE)
    {
        LOGE("Block size is too big %d > %d\n", number_of_bytes, MAXIMUM_SCRATCHPAD_BLOCK_SIZE);
        // Not necessary to send the request, directly return the mesh code
        return 6;
    }

    LOGD("Block_request: start_address = %d, number_of_bytes = %d\n", start_address, number_of_bytes);
    request.primitive_id = MSAP_SCRATCH_BLOCK_REQUEST;
    request.payload.msap_image_block_request_payload.start_add = start_address;
    request.payload.msap_image_block_request_payload.number_of_bytes = number_of_bytes;
    request.payload_length = sizeof(msap_image_block_req_pl_t) -
                             (MAXIMUM_SCRATCHPAD_BLOCK_SIZE - number_of_bytes);

    // Copy the block to the request
    memcpy(request.payload.msap_image_block_request_payload.bytes, bytes, number_of_bytes);

    res = WPC_Int_send_request_timeout(&request, &confirm, SCRATCHPAD_BLOK_TIMEOUT_MS);

    if (res < 0)
    {
        LOGE("Block request res=%d start_address=%d, number_of_bytes= %d\n", res, start_address, number_of_bytes);
        return res;
    }

    LOGD("Block request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_status_request(msap_scratchpad_status_conf_pl_t * status_p)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_STATUS_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    memcpy((uint8_t *) status_p,
           (uint8_t *) &confirm.payload.msap_scratchpad_status_confirm_payload,
           sizeof(msap_scratchpad_status_conf_pl_t));
    return APP_RES_OK;
}

int msap_scratchpad_update_request()
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_UPDATE_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGI("Update request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_clear_request()
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_CLEAR_REQUEST;
    request.payload_length = 0;

    // Use same timeout as scratchpad_start request that covers a scratchpad erase
    res = WPC_Int_send_request_timeout(&request, &confirm, SCRATCHPAD_START_TIMEOUT_MS);

    if (res < 0)
        return res;

    LOGI("Clear request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_target_write_request(uint8_t target_sequence,
                                         uint16_t target_crc,
                                         uint8_t action,
                                         uint8_t param)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_TARGET_WRITE_REQUEST;
    request.payload.msap_scratchpad_target_write_request_payload.target_sequence = target_sequence;
    request.payload.msap_scratchpad_target_write_request_payload.target_crc = target_crc;
    request.payload.msap_scratchpad_target_write_request_payload.action = action;
    request.payload.msap_scratchpad_target_write_request_payload.param = param;

    request.payload_length = sizeof(msap_scratchpad_write_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGD(" Target scratchpad request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}


int msap_scratchpad_target_read_request(uint8_t * target_sequence_p,
                                        uint16_t * target_crc_p,
                                        uint8_t * action_p,
                                        uint8_t * param_p)
{
    wpc_frame_t request, confirm;
    uint8_t result;
    int res;

    request.primitive_id = MSAP_SCRATCH_TARGET_READ_REQUEST;
    request.payload_length = 0;

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    result = confirm.payload.msap_scratchpad_target_read_confirm_payload.result;
    if (result == 0)
    {
        *target_sequence_p = confirm.payload.msap_scratchpad_target_read_confirm_payload.target_sequence;
        *target_crc_p =	confirm.payload.msap_scratchpad_target_read_confirm_payload.target_crc;
        *action_p = confirm.payload.msap_scratchpad_target_read_confirm_payload.action;
        *param_p = confirm.payload.msap_scratchpad_target_read_confirm_payload.param;
        LOGD("Target scratchpad Read : seq=%d crc=0x%x action=%d param=%d\n",
             *target_sequence_p,
             *target_crc_p,
             *action_p,
             *param_p);
    }
    else
    {
        LOGE("Try to read target scratchpad but it failed %d\n", result);
    }

    return result;
}

int msap_scratchpad_remote_status(app_addr_t destination_address)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_REMOTE_STATUS_REQUEST;
    request.payload.msap_image_remote_status_request_payload.target = destination_address;
    request.payload_length = sizeof(msap_image_remote_status_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGI("Remote status request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_scratchpad_remote_update(app_addr_t destination_address, uint8_t sequence, uint16_t delay_s)
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = MSAP_SCRATCH_REMOTE_UPDATE_REQUEST;
    request.payload.msap_image_remote_update_request_payload.target = destination_address;
    request.payload.msap_image_remote_update_request_payload.seq = sequence;
    request.payload.msap_image_remote_update_request_payload.delay_s = delay_s;
    request.payload_length = sizeof(msap_image_remote_update_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGD("Remote update request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

void msap_stack_state_indication_handler(msap_stack_state_ind_pl_t * payload)
{
    LOGI("Status is 0x%02x\n", payload->status);
    if (m_stack_status_cb != NULL)
    {
        m_stack_status_cb(payload->status);
    }
}

void msap_app_config_data_rx_indication_handler(msap_app_config_data_rx_ind_pl_t * payload)
{
    LOGD("Received config data : [%s] (%d)\n", payload->app_config_data, payload->diag_data_interval);
    if (m_app_conf_cb != NULL)
    {
        m_app_conf_cb(payload->sequence_number,
                      payload->diag_data_interval,
                      payload->app_config_data);
    }
}

void msap_image_remote_status_indication_handler(msap_image_remote_status_ind_pl_t * payload)
{
    app_scratchpad_status_t app_status;
    LOGI("Received remote status from %d\n", payload->source_address);
    if (m_remote_status_cb != NULL)
    {
        convert_internal_to_app_scratchpad_status(&app_status, &payload->status);
        m_remote_status_cb(payload->source_address, &app_status, payload->update_timeout);
    }
}

void msap_scan_nbors_indication_handler(msap_scan_nbors_ind_pl_t * payload)
{
    LOGI("Received scan neighbors ind res %d\n", payload->scan_ready);
    if (m_scan_neighbor_cb != NULL)
    {
        m_scan_neighbor_cb(payload->scan_ready);
    }
}

// Macro to avoid code duplication
#define REGISTER_CB(cb, internal_cb)   \
    ({                                 \
        bool res = true;               \
        do                             \
        {                              \
            Platform_lock_request();   \
            if (internal_cb != NULL)   \
                res = false;           \
            else                       \
                internal_cb = cb;      \
            Platform_unlock_request(); \
        } while (0);                   \
        res;                           \
    })

#define UNREGISTER_CB(internal_cb)       \
    ({                                   \
        bool res = true;                 \
        do                               \
        {                                \
            Platform_lock_request();     \
            res = (internal_cb != NULL); \
            internal_cb = NULL;          \
            Platform_unlock_request();   \
        } while (0);                     \
        res;                             \
    })

bool msap_register_for_app_config(onAppConfigDataReceived_cb_f cb)
{
    return REGISTER_CB(cb, m_app_conf_cb);
}

bool msap_unregister_from_app_config()
{
    return UNREGISTER_CB(m_app_conf_cb);
}

bool msap_register_for_remote_status(onRemoteStatus_cb_f cb)
{
    return REGISTER_CB(cb, m_remote_status_cb);
}

bool msap_unregister_from_remote_status()
{
    return UNREGISTER_CB(m_remote_status_cb);
}

bool msap_register_for_scan_neighbors_done(onScanNeighborsDone_cb_f cb)
{
    return REGISTER_CB(cb, m_scan_neighbor_cb);
}

bool msap_unregister_from_scan_neighbors_done()
{
    return UNREGISTER_CB(m_scan_neighbor_cb);
}

bool msap_register_for_stack_status(onStackStatusReceived_cb_f cb)
{
    return REGISTER_CB(cb, m_stack_status_cb);
}

bool msap_unregister_from_stack_status()
{
    return UNREGISTER_CB(m_stack_status_cb);
}
