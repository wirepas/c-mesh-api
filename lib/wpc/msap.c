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
 * \brief   Registered callback for scan neighbors
 */
static onScanNeighborsDone_cb_f m_scan_neighbor_cb = NULL;

/**
 * \brief   Registered callback for stack status neighbors
 */
static onStackStatusReceived_cb_f m_stack_status_cb = NULL;

/**
 * \brief   Registered callback for config data item
 */
static onConfigDataItemReceived_cb_f m_config_data_item_cb = NULL;

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
#define SCRATCHPAD_BLOCK_TIMEOUT_MS 5000

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

    res = confirm.payload.sap_generic_confirm_payload.result;

    LOGI("Start request result = 0x%02x\n", res);

    // Dualmcu app is not generating StackStarted event
    // Emulate it until it is fixed.
    if (res == 0 && m_stack_status_cb != NULL)
    {
        m_stack_status_cb(0);
    }
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

int msap_app_config_data_write_request(uint8_t seq, uint16_t interval, const uint8_t * config_p, uint8_t size)
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

        LOGD("App config : seq=%d, interval=%d\n", *seq, *interval);
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

int msap_scratchpad_block_request(uint32_t start_address, uint8_t number_of_bytes, const uint8_t * bytes)
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

    res = WPC_Int_send_request_timeout(&request, &confirm, SCRATCHPAD_BLOCK_TIMEOUT_MS);

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

    request.payload_length = sizeof(msap_scratchpad_target_write_req_pl_t);

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
        *target_sequence_p =
            confirm.payload.msap_scratchpad_target_read_confirm_payload.target_sequence;
        *target_crc_p =
            confirm.payload.msap_scratchpad_target_read_confirm_payload.target_crc;
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

int msap_scratchpad_block_read_request(uint32_t start_address, uint8_t number_of_bytes, uint8_t * bytes)
{
    wpc_frame_t request, confirm;
    int res;

    if (number_of_bytes > MAXIMUM_SCRATCHPAD_BLOCK_SIZE)
    {
        LOGE("Block size is too big %d > %d\n", number_of_bytes, MAXIMUM_SCRATCHPAD_BLOCK_SIZE);
        // Not necessary to send the request, directly return the mesh code
        return 6;
    }

    LOGD("Read_request: start_address = %d, number_of_bytes = %d\n", start_address, number_of_bytes);
    request.primitive_id = MSAP_SCRATCH_BLOCK_READ_REQUEST;
    request.payload.msap_image_block_read_request_payload.start_add = start_address;
    request.payload.msap_image_block_read_request_payload.number_of_bytes = number_of_bytes;
    request.payload_length = sizeof(msap_image_block_read_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
    {
        LOGE("Read request res=%d start_address=%d, number_of_bytes= %d\n", res, start_address, number_of_bytes);
        return res;
    }

    if (confirm.payload.msap_image_block_read_confirm_payload.result == 0)
    {
        // Copy the block from the confirmation
        memcpy(bytes, confirm.payload.msap_image_block_read_confirm_payload.bytes, number_of_bytes);
    }

    LOGD("Read request result = 0x%02x\n",
         confirm.payload.msap_image_block_read_confirm_payload.result);
    return confirm.payload.msap_image_block_read_confirm_payload.result;
}

int msap_config_data_item_set_request(const uint16_t endpoint,
                                      const uint8_t *const payload,
                                      const uint8_t payload_size)
{
    if (payload_size > MAXIMUM_CDC_ITEM_PAYLOAD_SIZE)
    {
        LOGE("Too large payload size (%d) for config data item\n", payload_size);
        return WPC_INT_WRONG_PARAM_ERROR;
    }

    wpc_frame_t request = {
        .primitive_id = MSAP_CONFIG_DATA_ITEM_SET_REQUEST,
        .payload_length = sizeof(msap_config_data_item_set_req_pl_t),
        .payload = {
            .msap_config_data_item_set_request_payload = {
                .endpoint = endpoint,
                .payload_length = payload_size,
                .payload = {0}
            }
        }
    };

    memcpy(request.payload.msap_config_data_item_set_request_payload.payload,
           payload,
           payload_size);

    wpc_frame_t confirm;
    const int res = WPC_Int_send_request(&request, &confirm);
    if (res < 0)
    {
        return res;
    }

    return confirm.payload.sap_generic_confirm_payload.result;
}

int msap_config_data_item_get_request(const uint16_t endpoint,
                                      uint8_t *const payload,
                                      const size_t payload_capacity,
                                      uint8_t *const payload_size)
{
    wpc_frame_t request = {
        .primitive_id = MSAP_CONFIG_DATA_ITEM_GET_REQUEST,
        .payload_length = sizeof(msap_config_data_item_get_req_pl_t),
        .payload = {
            .msap_config_data_item_get_request_payload = {
                .endpoint = endpoint,
            }
        }
    };

    wpc_frame_t confirm;
    const int res = WPC_Int_send_request(&request, &confirm);
    if (res < 0)
    {
        return res;
    }

    if (confirm.payload.sap_generic_confirm_payload.result == 0)
    {
        const uint8_t payload_length = confirm.payload.msap_config_data_item_get_confirm_payload.payload_length;
        if (payload_length > payload_capacity)
        {
            return WPC_INT_WRONG_BUFFER_SIZE;
        }
        if (payload_length > sizeof(confirm.payload.msap_config_data_item_get_confirm_payload.payload))
        {
            return WPC_INT_WRONG_PARAM_ERROR;
        }

        memcpy(payload,
               &confirm.payload.msap_config_data_item_get_confirm_payload.payload,
               payload_length);
        *payload_size = payload_length;
    }

    return confirm.payload.sap_generic_confirm_payload.result;
}

void msap_stack_state_indication_handler(msap_stack_state_ind_pl_t * payload)
{
    LOGI("Status is 0x%02x\n", payload->status);
    if (m_stack_status_cb != NULL)
    {
        m_stack_status_cb(payload->status);
    }

    WPC_Int_set_mtu();
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

void msap_scan_nbors_indication_handler(msap_scan_nbors_ind_pl_t * payload)
{
    LOGI("Received scan neighbors ind res %d\n", payload->scan_ready);
    if (m_scan_neighbor_cb != NULL)
    {
        m_scan_neighbor_cb(payload->scan_ready);
    }
}

void msap_config_data_item_rx_indication_handler(msap_config_data_item_rx_ind_pl_t * payload)
{
    LOGD("Received configuration data item indication for endpoint: 0x%04X\n",
         payload->endpoint);

    if (m_config_data_item_cb != NULL)
    {
        m_config_data_item_cb(payload->endpoint,
                              payload->payload,
                              payload->payload_length);

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

bool msap_register_for_config_data_item(onConfigDataItemReceived_cb_f cb)
{
    return REGISTER_CB(cb, m_config_data_item_cb);
}

bool msap_unregister_from_config_data_item()
{
    return UNREGISTER_CB(m_config_data_item_cb);
}
