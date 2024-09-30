/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include "proto_config.h"
#include "proto_otap.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "platform.h"
#include "common.h"

#define LOG_MODULE_NAME "config_proto"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Wirepas Gateway's protobuff message definition version
// see lib/wpc_proto/deps/backend-apis/gateway_to_backend/README.md
#define GW_PROTO_MESSAGE_VERSION 1

// API version implemented in the gateway
// to fill implemented_api_version in GatewayInfo
// see lib/wpc_proto/deps/backend-apis/gateway_to_backend/protocol_buffers_files/config_message.proto
#define GW_PROTO_API_VERSION 2

/** Structure to hold config from node */
typedef struct sink_config
{
    app_addr_t node_address;
    net_addr_t network_address;
    bool CipherKeySet;
    bool AuthenticationKeySet;
    uint16_t stack_profile;
    uint16_t hw_magic;
    uint16_t ac_limit_min;
    uint16_t ac_limit_max;
    uint16_t app_config_max_size;
    uint16_t version[4];
    uint16_t ac_range_min_cur;  // 0 means unset ?
    uint16_t ac_range_max_cur;
    uint8_t max_mtu;
    uint8_t ch_range_min;
    uint8_t ch_range_max;
    uint8_t pdu_buffer_size;
    uint8_t StackStatus;
    net_channel_t network_channel;
    app_role_t app_node_role;
    app_scratchpad_status_t otap_status;
    uint8_t target_sequence;
    uint16_t target_crc;
    uint8_t target_action;
    uint8_t target_param;
} sink_config_t;

/* TODO : Protect config access */

/** Sink config storage */
static sink_config_t m_sink_config;

/* values for delay unit in MSAP scratchpad action */
typedef enum
{
    delay_minute = 0b1,
    delay_hour = 0b10,
    delay_day = 0b11,
} delay_unit;

/** return the value of the delay param used in MSAP for scratchpad action (see @delay_unit) */
#define GET_DELAY(unit, amount) (((unit) << 6) + (amount))

/** Special event status value
 * to use when generating internally a status event with @onStackStatusReceived */
#define IGNORE_STATUS 0xFF

static onEventStatus_cb_f m_onProtoEventStatus_cb = NULL;

/* Typedef used to avoid warning at compile time */
typedef app_res_e (*func_1_param)(void * param);
typedef app_res_e (*func_2_param)(void * param1, void * param2);

/**
 * \brief   Generic function to read one or two parameters from node
 * \param   f
 *          The function to call to get the parameters (type func_1_t or  func_2_t)
 * \param   var1
 *          Pointer to first parameter
 * \param   var2
 *          Pointer to second parameter (can be NULL)
 * \param   var_name
 *          Parameter name
 * \return  True if correctly read, false otherwise
 */
static bool get_value_from_node(void * f,
                                void * var1,
                                void * var2,
                                char * var_name)
{
    app_res_e res = APP_RES_INTERNAL_ERROR;

    if (var2 == NULL)
    {
        res = ((func_1_param) f)(var1);
    }
    else
    {
        res = ((func_2_param) f)(var1, var2);
    }

    if (res != APP_RES_OK)
    {
        LOGE("Cannot get %s from node\n", var_name);
        return false;
    }

    return true;
}

static app_role_t convert_role_to_app_format(wp_NodeRole * proto_role)
{
    app_role_t options = 0;
    int count = proto_role->flags_count;
    while (count > 0)
    {
        count--;
        options |= (proto_role->flags[count] == wp_NodeRole_RoleFlags_LOW_LATENCY)
                   ? APP_ROLE_OPTION_LL : 0;
        options |= (proto_role->flags[count] == wp_NodeRole_RoleFlags_AUTOROLE)
                   ? APP_ROLE_OPTION_AUTOROLE : 0;
    }

    return CREATE_ROLE(proto_role->role, options);
}

static void convert_role_to_proto_format(app_role_t role, wp_NodeRole * proto_role)
{
    _Static_assert(member_size(wp_NodeRole, flags) >= (2 * sizeof(wp_NodeRole_RoleFlags)), "Too many role flags");

    pb_size_t flags_count = 0;
    proto_role->role = GET_BASE_ROLE(role);
    if (GET_ROLE_OPTIONS(role) & APP_ROLE_OPTION_LL)
    {
        proto_role->flags[flags_count] = wp_NodeRole_RoleFlags_LOW_LATENCY;
        flags_count++;
    }
    if (GET_ROLE_OPTIONS(role) & APP_ROLE_OPTION_AUTOROLE)
    {
        proto_role->flags[flags_count] = wp_NodeRole_RoleFlags_AUTOROLE;
        flags_count++;
    }
    proto_role->flags_count = flags_count;
}

static wp_ScratchpadType convert_scrat_type_to_proto_format(uint8_t scrat_type)
{
    switch (scrat_type)
    {
        case 1:
            return wp_ScratchpadType_PRESENT;
        case 2:
            return wp_ScratchpadType_PROCESS;
        case 0:
        default:
            return wp_ScratchpadType_BLANK;
    }
}

static wp_ScratchpadStatus convert_scrat_status_to_proto_format(uint8_t scrat_status)
{
    switch (scrat_status)
    {
        case 0:
            return wp_ScratchpadStatus_SUCCESS;
        case 255:
            return wp_ScratchpadStatus_NEW;
        default:
            return wp_ScratchpadStatus_ERROR;
    }
}

static wp_ProcessingDelay convert_delay_to_proto_format(uint8_t delay)
{
    switch (delay) {
        case GET_DELAY(delay_minute, 10):
            return wp_ProcessingDelay_TEN_MINUTES;
        case GET_DELAY(delay_minute, 30):
            return wp_ProcessingDelay_THIRTY_MINUTES;
        case GET_DELAY(delay_hour, 1):
            return wp_ProcessingDelay_ONE_HOUR;
        case GET_DELAY(delay_hour, 6):
            return wp_ProcessingDelay_SIX_HOURS;
        case GET_DELAY(delay_day, 1):
            return wp_ProcessingDelay_ONE_DAY;
        case GET_DELAY(delay_day, 2):
            return wp_ProcessingDelay_TWO_DAYS;
        case GET_DELAY(delay_day, 5):
            return wp_ProcessingDelay_FIVE_DAYS;
        default:
            return wp_ProcessingDelay_UNKNOWN_DELAY;
    }
}

uint8_t convert_delay_to_app_format(wp_ProcessingDelay delay)
{
    switch (delay) {
    case wp_ProcessingDelay_TEN_MINUTES:
        return GET_DELAY(delay_minute, 10);
    case wp_ProcessingDelay_THIRTY_MINUTES:
        return GET_DELAY(delay_minute, 30);
    case wp_ProcessingDelay_ONE_HOUR:
        return GET_DELAY(delay_hour, 1);
    case wp_ProcessingDelay_SIX_HOURS:
        return GET_DELAY(delay_hour, 6);
    case wp_ProcessingDelay_ONE_DAY:
        return GET_DELAY(delay_day, 1);
    case wp_ProcessingDelay_TWO_DAYS:
        return GET_DELAY(delay_day, 2);
    case wp_ProcessingDelay_FIVE_DAYS:
        return GET_DELAY(delay_day, 5);
    default:
        return 0;
    }
}

static bool initialize_otap_variables()
{
    bool res = true;

    if (WPC_get_local_scratchpad_status(&m_sink_config.otap_status)
        != APP_RES_OK)
    {
        LOGE("Cannot get local scratchpad status\n");
        res = false;
    }

    if (WPC_read_target_scratchpad(&m_sink_config.target_sequence,
                                   &m_sink_config.target_crc,
                                   &m_sink_config.target_action,
                                   &m_sink_config.target_param) != APP_RES_OK)
    {
        LOGE("Cannot get target scratchpad status\n");
        res = false;
    }

    return res;
}

static bool initialize_config_variables()
{
    bool res = true;

    res &= get_value_from_node(WPC_get_stack_profile, &m_sink_config.stack_profile, NULL,
                               "Stack profile");
    res &= get_value_from_node(WPC_get_hw_magic, &m_sink_config.hw_magic, NULL,
                               "Hw magic");
    res &= get_value_from_node(WPC_get_mtu, &m_sink_config.max_mtu, NULL,
                               "MTU");
    res &= get_value_from_node(WPC_get_pdu_buffer_size, &m_sink_config.pdu_buffer_size, NULL,
                               "PDU Buffer Size");
    res &= get_value_from_node(WPC_get_channel_limits, &m_sink_config.ch_range_min, &m_sink_config.ch_range_max,
                               "Channel Range");
    res &= get_value_from_node(WPC_get_access_cycle_limits, &m_sink_config.ac_limit_min, &m_sink_config.ac_limit_max,
                               "AC Range");
    res &= get_value_from_node(WPC_get_app_config_data_size, &m_sink_config.app_config_max_size, NULL,
                               "App Config Max size");
    res &= get_value_from_node(WPC_is_cipher_key_set, &m_sink_config.CipherKeySet, NULL,
                               "Cipher key set");
    res &= get_value_from_node(WPC_is_authentication_key_set, &m_sink_config.AuthenticationKeySet, NULL,
                               "Authentication key set");
    res &= get_value_from_node(WPC_get_stack_status, &m_sink_config.StackStatus, NULL,
                               "Stack Status");
    res &= get_value_from_node(WPC_get_access_cycle_range, &m_sink_config.ac_range_min_cur, &m_sink_config.ac_range_max_cur,
                               "Current access cycle range");
    res &= get_value_from_node(WPC_get_node_address, &m_sink_config.node_address, NULL,
                               "Network address");
    res &= get_value_from_node(WPC_get_role, &m_sink_config.app_node_role, NULL,
                               "Node role");
    res &= get_value_from_node(WPC_get_network_address, &m_sink_config.network_address, NULL,
                               "Network address");
    res &= get_value_from_node(WPC_get_network_channel, &m_sink_config.network_channel, NULL,
                               "Network channel");

    if (WPC_get_firmware_version(m_sink_config.version) == APP_RES_OK)
    {
        LOGI("Stack version is: %d.%d.%d.%d\n",
             m_sink_config.version[0],
             m_sink_config.version[1],
             m_sink_config.version[2],
             m_sink_config.version[3]);
    }
    else
    {
        res = false;
    }

    return res;
}

static void fill_target_and_action(wp_TargetScratchpadAndAction * target_and_action_p)
{
    *target_and_action_p = (wp_TargetScratchpadAndAction){
        .action = m_sink_config.target_action + wp_ScratchpadAction_NO_OTAP, // NO_OTAP = 0 on CMeshAPI
        .has_target_sequence = true, // do we need to check action to set it true/false ?
        .target_sequence = m_sink_config.target_sequence,
        .has_target_crc = true, // do we need to check action to set it true/false ?
        .target_crc = m_sink_config.target_crc
    };

    // Set param if target is PROPAGATE_AND_PROCESS_WITH_DELAY
    if (m_sink_config.target_action == 3)
    {
        wp_ProcessingDelay delay = convert_delay_to_proto_format(m_sink_config.target_param);
        if (delay != wp_ProcessingDelay_UNKNOWN_DELAY)
        {
            target_and_action_p->which_param = wp_TargetScratchpadAndAction_delay_tag;
            target_and_action_p->param.delay = delay;
        }
        else
        {
            target_and_action_p->which_param = wp_TargetScratchpadAndAction_raw_tag;
            target_and_action_p->param.raw = m_sink_config.target_param;
        }
    }

}

static void fill_sink_read_config(wp_SinkReadConfig * config_p)
{
    _Static_assert(     member_size(wp_AppConfigData_app_config_data_t, bytes)
                     >= member_size(msap_app_config_data_write_req_pl_t, app_config_data),
                      "wp_AppConfigData_app_config_data_t is too small");
    _Static_assert(member_size(wp_SinkReadConfig, sink_id) >= SINK_ID_MAX_SIZE, "wp_SinkReadConfig is too small");

    uint8_t status = m_sink_config.StackStatus;

    *config_p = (wp_SinkReadConfig){
        /* Sink minimal config */
        .has_node_role = true,
        .has_node_address =  true,
        .node_address = m_sink_config.node_address,
        .has_network_address = true,
        .network_address = m_sink_config.network_address,
        .has_network_channel = true,
        .network_channel = m_sink_config.network_channel,
        .has_app_config = true,
        .has_channel_map = false,
        .has_are_keys_set = true,
        .are_keys_set =    m_sink_config.CipherKeySet
                        && m_sink_config.AuthenticationKeySet,

        .has_current_ac_range = (m_sink_config.ac_range_min_cur == 0 ? false : true),

        .current_ac_range = {.min_ms = m_sink_config.ac_range_min_cur,
                             .max_ms = m_sink_config.ac_range_max_cur},
        /* Read only parameters */
        .has_ac_limits = true,
        .ac_limits = {.min_ms = m_sink_config.ac_limit_min,
                      .max_ms = m_sink_config.ac_limit_max},
        .has_max_mtu = true,
        .max_mtu = m_sink_config.max_mtu,
        .has_channel_limits = true,
        .channel_limits = {.min_channel = m_sink_config.ch_range_min,
                           .max_channel = m_sink_config.ch_range_max},
        .has_hw_magic = true,
        .hw_magic = m_sink_config.hw_magic,
        .has_stack_profile = true,
        .stack_profile = m_sink_config.stack_profile,
        .has_app_config_max_size = true,
        .app_config_max_size = m_sink_config.app_config_max_size,
        .has_firmware_version = true,
        .firmware_version = {.major = m_sink_config.version[0],
                             .minor = m_sink_config.version[1],
                             .maint = m_sink_config.version[2],
                             .dev = m_sink_config.version[3] },
        /* State of sink */
        .has_sink_state = true,
        .sink_state = ((status & APP_STACK_STOPPED) ? wp_OnOffState_OFF : wp_OnOffState_ON),
        /* Scratchpad info for the sink */
        .has_stored_scratchpad = true,
        .stored_scratchpad = { .len = m_sink_config.otap_status.scrat_len,
                               .crc = m_sink_config.otap_status.scrat_crc,
                               .seq = m_sink_config.otap_status.scrat_seq_number, },
        .has_stored_status = true,
        .stored_status = convert_scrat_status_to_proto_format(m_sink_config.otap_status.scrat_status),
        .has_stored_type = true,
        .stored_type = convert_scrat_type_to_proto_format(m_sink_config.otap_status.scrat_type),
        .has_processed_scratchpad = true,
        .processed_scratchpad = { .len = m_sink_config.otap_status.processed_scrat_len,
                                  .crc = m_sink_config.otap_status.processed_scrat_crc,
                                  .seq = m_sink_config.otap_status.processed_scrat_seq_number, },
        .has_firmware_area_id = true,
        .firmware_area_id = m_sink_config.otap_status.firmware_memory_area_id,
        .has_target_and_action = true,
    };

    strncpy(config_p->sink_id, Common_get_sink_id(), SINK_ID_MAX_SIZE);

    convert_role_to_proto_format(m_sink_config.app_node_role, &config_p->node_role);

    fill_target_and_action(&config_p->target_and_action);

    if (MAXIMUM_APP_CONFIG_SIZE >= m_sink_config.app_config_max_size)
    {
        uint16_t diag_data_interval = 0;  // needed to avoid trouble with pointer alignement
        uint8_t seq = 0;
        config_p->app_config.app_config_data.size = m_sink_config.app_config_max_size;
        if (WPC_get_app_config_data(&seq,
                                    &diag_data_interval,
                                    config_p->app_config.app_config_data.bytes,
                                    member_size(wp_AppConfigData_app_config_data_t, bytes))
            != APP_RES_OK)
        {
            LOGE("Cannot get App config data from node\n");
            config_p->has_app_config = false;
            config_p->app_config.app_config_data.size = 0;
        }
        config_p->app_config.seq = seq;
        config_p->app_config.diag_interval_s = diag_data_interval;
    }
}

static void fill_status_event(wp_StatusEvent * status_event_p,
                                 wp_OnOffState state,
                                 pb_size_t config_count)
{
    _Static_assert(member_size(wp_StatusEvent, gw_model) >= GATEWAY_MODEL_MAX_SIZE, "Gateway model too big");
    _Static_assert(member_size(wp_StatusEvent, gw_version) >= GATEWAY_VERSION_MAX_SIZE, "Gateway version too big");

    *status_event_p = (wp_StatusEvent){
        .version = GW_PROTO_MESSAGE_VERSION,
        .state = state,
        .configs_count = config_count,
        .has_gw_model = (strlen(Common_get_gateway_model()) != 0),
        .has_gw_version = (strlen(Common_get_gateway_version()) != 0),
        .has_max_scratchpad_size = true,
        .max_scratchpad_size = member_size(wp_UploadScratchpadReq_scratchpad_t, bytes)
    };

    // Add current config for online node
    if (config_count == 1)
    {
        fill_sink_read_config(&status_event_p->configs[0]);
    }

    strncpy(status_event_p->gw_model, Common_get_gateway_model(), GATEWAY_MODEL_MAX_SIZE);
    strncpy(status_event_p->gw_version, Common_get_gateway_version(), GATEWAY_VERSION_MAX_SIZE);

    Common_fill_event_header(&status_event_p->header);
}

static bool refresh_full_stack_state()
{
    bool res = true;

    uint8_t previous_status = m_sink_config.StackStatus;

    /* Read config from sink */
    if (!initialize_config_variables())
    {
        LOGE("All the settings cannot be read\n");
        res = false;
    }

    /* Read otap config from sink */
    if (!initialize_otap_variables())
    {
        LOGE("All the otap settings cannot be read\n");
        res = false;
    }

    if(   (previous_status & APP_STACK_STOPPED)
       != (m_sink_config.StackStatus & APP_STACK_STOPPED) )
    {
        // Stack state changed
        if ((m_sink_config.StackStatus & APP_STACK_STOPPED) == 0)
        {
            LOGI("Refresh : Stack started %d\n", m_sink_config.StackStatus);
        }
        else
        {
            LOGI("Refresh : Stack stopped %d\n", m_sink_config.StackStatus);
        }
    }

    return res;
}

static void onStackStatusReceived(uint8_t status)
{
    bool res;
    wp_StatusEvent * message_StatusEvent_p;
    uint8_t * encoded_message_p;
    wp_GenericMessage message = wp_GenericMessage_init_zero;
    wp_WirepasMessage message_wirepas = wp_WirepasMessage_init_zero;
    message.wirepas = &message_wirepas;

    if (status == IGNORE_STATUS)
    {
        LOGI("Evt : Stack status event generated %d\n", m_sink_config.StackStatus);
    }
    else
    {
        LOGI("Evt : Stack status received : %d %d\n", status, m_sink_config.StackStatus);
    }

    (void) refresh_full_stack_state();

    LOGI("Evt : Stack status updated : %d\n", m_sink_config.StackStatus);

    // Allocate the needed space for only the submessage we want to send
    message_StatusEvent_p = Platform_malloc(sizeof(wp_StatusEvent));
    if (message_StatusEvent_p == NULL)
    {
        LOGE("Not enough memory to encode StatusEvent\n");
        return;
    }

    // Allocate needed buffer for encoded message
    encoded_message_p = Platform_malloc(WPC_PROTO_MAX_RESPONSE_SIZE);
    if (encoded_message_p == NULL)
    {
        LOGE("Not enough memory for output buffer");
        Platform_free(message_StatusEvent_p, sizeof(wp_StatusEvent));
        return;
    }

    message_wirepas.status_event = message_StatusEvent_p;

    fill_status_event(message_StatusEvent_p, wp_OnOffState_ON, 1);

    // Using the module static buffer
    pb_ostream_t stream = pb_ostream_from_buffer(encoded_message_p, WPC_PROTO_MAX_RESPONSE_SIZE);

    /* Now we are ready to encode the message! */
    res = pb_encode(&stream, wp_GenericMessage_fields, &message);

    /* Release buffer as we don't need it anymore */
    Platform_free(message_StatusEvent_p, sizeof(wp_StatusEvent));

	if (!res) {
		LOGE("Encoding failed: %s\n", PB_GET_ERROR(&stream));
	}
	else
	{
        LOGI("Msg size %d\n", stream.bytes_written);
        if (m_onProtoEventStatus_cb != NULL)
        {
            m_onProtoEventStatus_cb(encoded_message_p, stream.bytes_written);
        }
    }

    Platform_free(encoded_message_p, WPC_PROTO_MAX_RESPONSE_SIZE);
}

bool Proto_config_init(void)
{
    /* Read initial config from sink */
    if (!refresh_full_stack_state())
    {
        // do not return false, just continue init
    }

    if (WPC_register_for_stack_status(onStackStatusReceived) != APP_RES_OK)
    {
        LOGE("Stack status already registered\n");
        return false;
    }

    return true;
}

void Proto_config_close()
{
}

app_proto_res_e Proto_config_handle_get_scratchpad_status(wp_GetScratchpadStatusReq *req,
                                                          wp_GetScratchpadStatusResp *resp)
{
    app_res_e res = APP_RES_OK;

    // TODO: Add some sanity checks

    *resp = (wp_GetScratchpadStatusResp){
        .has_stored_scratchpad = true,
        .stored_scratchpad = { .len = m_sink_config.otap_status.scrat_len,
                               .crc = m_sink_config.otap_status.scrat_crc,
                               .seq = m_sink_config.otap_status.scrat_seq_number, },
        .has_stored_status = true,
        .stored_status = convert_scrat_status_to_proto_format(m_sink_config.otap_status.scrat_status),
        .has_stored_type = true,
        .stored_type = convert_scrat_type_to_proto_format(m_sink_config.otap_status.scrat_type),
        .has_processed_scratchpad = true,
        .processed_scratchpad = { .len = m_sink_config.otap_status.processed_scrat_len,
                                  .crc = m_sink_config.otap_status.processed_scrat_crc,
                                  .seq = m_sink_config.otap_status.processed_scrat_seq_number, },
        .has_firmware_area_id = true,
        .firmware_area_id = m_sink_config.otap_status.firmware_memory_area_id,
        .has_target_and_action = true,
    };

    fill_target_and_action(&resp->target_and_action);

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(res));

    return APP_RES_PROTO_OK;
}


app_proto_res_e Proto_config_handle_set_config(wp_SetConfigReq *req,
                                               wp_SetConfigResp *resp)
{
    app_res_e res;
    app_res_e global_res = APP_RES_OK;
    bool config_has_changed = false;
    bool restart_stack = false;
    bool stop_stack = false;
    wp_SinkNewConfig * cfg = &req->config;

    // TODO: Add some sanity checks

    if (strcmp(cfg->sink_id, Common_get_sink_id()) != 0)
    {
        // wrong target, exit
        LOGE("Wrong sink target\n");
        Common_Fill_response_header(&resp->header,
                                    req->header.req_id,
                                    wp_ErrorCode_INVALID_SINK_ID);
        // Config reponse not filled in this case
        return APP_RES_PROTO_OK;
    }

    // At least refresh stack status
    WPC_get_stack_status(&m_sink_config.StackStatus);

    LOGI("Config received : %d, %d, %d, %d, %d, %d, %d/%d\n",
            cfg->has_node_role, cfg->has_node_address, cfg->has_network_address,
            cfg->has_network_channel, cfg->has_keys, cfg->has_current_ac_range,
            cfg->has_sink_state, cfg->sink_state);

    if ((m_sink_config.StackStatus & APP_STACK_STOPPED))
    {
        // stack already stopped, check if start will be needed
        if (cfg->has_sink_state && (cfg->sink_state == wp_OnOffState_ON) )
        {
            restart_stack = true;
            config_has_changed = true;
        }
    }
    else
    {
        // stack running, check if stop is needed to apply config
        if  (    cfg->has_node_role
              || cfg->has_node_address
              || cfg->has_network_address
              || cfg->has_network_channel
              || cfg->has_keys
              || cfg->has_current_ac_range )
        {
            restart_stack = true;
            stop_stack = true;
        }
        // check if stop is requested
        if (cfg->has_sink_state && (cfg->sink_state == wp_OnOffState_OFF) )
        {
            restart_stack = false;
            stop_stack = true;
            config_has_changed = true;
        }
    }

    LOGI("Stack actions : stop %d, restart %d, \n", stop_stack, restart_stack);

    if (stop_stack)
    {
        // stack needs to be stopped before applying config
        WPC_set_autostart(0);
        res = WPC_stop_stack();
        if (res != APP_RES_OK)
        {
            LOGE("Stack stop failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            m_sink_config.StackStatus |= APP_STACK_STOPPED;
            LOGI("SetConf : Stack stopped %d\n", m_sink_config.StackStatus);
            config_has_changed = true;
        }
    }

    // role must be changed first
    if (cfg->has_node_role)
    {
        app_role_t new_role = convert_role_to_app_format(&cfg->node_role);
        if (new_role != m_sink_config.app_node_role)
        {
            res = WPC_set_role(new_role);
            if (res != APP_RES_OK)
            {
                LOGE("Set role failed\n");
                global_res = APP_RES_INVALID_VALUE;
            }
            else
            {
                LOGI("Set role 0x%02X\n", new_role);
                m_sink_config.app_node_role = new_role;
                config_has_changed = true;
            }
        }
    }

    if ( cfg->has_node_address &&
        (cfg->node_address != m_sink_config.node_address))
    {
        res = WPC_set_node_address(cfg->node_address);
        if (res != APP_RES_OK)
        {
            LOGE("Set node address failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set node address %d\n", cfg->node_address);
            m_sink_config.node_address = cfg->node_address;
            config_has_changed = true;
        }
    }

    if ( cfg->has_network_address &&
        (cfg->network_address != m_sink_config.network_address))
    {
        res = WPC_set_network_address(cfg->network_address);
        if (res != APP_RES_OK)
        {
            LOGE("Set network address failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set network address %d\n", cfg->network_address);
            m_sink_config.network_address = cfg->network_address;
            config_has_changed = true;
        }
    }

    if ( cfg->has_network_channel &&
        (cfg->network_channel != m_sink_config.network_channel))
    {
        res = WPC_set_network_channel(cfg->network_channel);
        if (res != APP_RES_OK)
        {
            LOGE("Set network channel failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set network channel %d\n", cfg->network_channel);
            m_sink_config.network_channel = cfg->network_channel;
            config_has_changed = true;
        }
    }

    if (cfg->has_app_config)
    {
        // no check, just apply it
        res = WPC_set_app_config_data(cfg->app_config.seq,
                                      cfg->app_config.diag_interval_s,
                                      cfg->app_config.app_config_data.bytes,
                                      cfg->app_config.app_config_data.size);
        if (res != APP_RES_OK)
        {
            LOGE("Set app config failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set app config\n");
            config_has_changed = true;
        }
    }

    if (cfg->has_keys)
    {
        res = WPC_set_cipher_key(cfg->keys.cipher);
        if (res != APP_RES_OK)
        {
            LOGE("Set Cipher key failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set Cipher key\n");
            WPC_is_cipher_key_set(&m_sink_config.CipherKeySet);
            config_has_changed = true;
        }

        res = WPC_set_authentication_key(cfg->keys.authentication);
        if (res != APP_RES_OK)
        {
            LOGE("Set Authentication key failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            LOGI("Set Authentication key\n");
            WPC_is_authentication_key_set(&m_sink_config.AuthenticationKeySet);
            config_has_changed = true;
        }
    }

    if (cfg->has_current_ac_range)
    {
        if ((cfg->current_ac_range.min_ms != m_sink_config.ac_range_min_cur)
            || (cfg->current_ac_range.max_ms != m_sink_config.ac_range_max_cur))
        {
            res = WPC_set_access_cycle_range(cfg->current_ac_range.min_ms,
                                             cfg->current_ac_range.max_ms);
            if (res != APP_RES_OK)
            {
                LOGE("Set AC range failed\n");
                global_res = APP_RES_INVALID_VALUE;
            }
            else
            {
                LOGI("Set AC range %d-%d\n", cfg->current_ac_range.min_ms,
                                             cfg->current_ac_range.max_ms);
                m_sink_config.ac_range_min_cur = cfg->current_ac_range.min_ms;
                m_sink_config.ac_range_max_cur = cfg->current_ac_range.max_ms;
                config_has_changed = true;
            }
        }
    }

    if (restart_stack)
    {
        // Start the stack
        res = WPC_start_stack();
        if (res != APP_RES_OK)
        {
            LOGE("Stack start failed\n");
            global_res = APP_RES_INVALID_VALUE;
        }
        else
        {
            WPC_set_autostart(1);
            m_sink_config.StackStatus &= ~(uint8_t) APP_STACK_STOPPED;
            LOGI("SetConf : Stack started %d\n", m_sink_config.StackStatus);
        }
    }

    if (config_has_changed)
    {
        // Force config refresh and send event status
        onStackStatusReceived(IGNORE_STATUS);
    }

    if (global_res != APP_RES_OK)
    {
        LOGE("WPC_set_config failed, res=%d\n", global_res);
    }
    else
    {
        LOGI("WPC_set_config success\n");
    }

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(global_res));
    fill_sink_read_config(&resp->config);

    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_config_handle_get_configs(wp_GetConfigsReq *req,
                                                wp_GetConfigsResp *resp)
{
    app_res_e res = APP_RES_OK;

    // TODO: Add some sanity checks

    // At least refresh stack status
    res = WPC_get_stack_status(&m_sink_config.StackStatus);
    if (res != APP_RES_OK)
    {
        LOGE("WPC_get_config failed, res=%d\n",res);
    }
    else
    {
        LOGI("WPC_get_config success\n");
    }

    // as config is available in cache, response is ok
    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                wp_ErrorCode_OK);
    resp->configs_count = 1;
    fill_sink_read_config(&resp->configs[0]);

    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_config_handle_get_gateway_info_request(wp_GetGwInfoReq * req,
                                                             wp_GetGwInfoResp * resp)
{
    // TODO: Add some sanity checks

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                wp_ErrorCode_OK);

    resp->info.current_time_s_epoch = Platform_get_timestamp_ms_epoch();

    resp->info.has_gw_model = (Common_get_gateway_model() != 0);
    strcpy(resp->info.gw_model, Common_get_gateway_model());
    resp->info.has_gw_version = (Common_get_gateway_version() != 0);
    strcpy(resp->info.gw_version, Common_get_gateway_version());

    resp->info.has_implemented_api_version = true;
    resp->info.implemented_api_version = GW_PROTO_API_VERSION;

    resp->info.has_max_scratchpad_size = true,
    resp->info.max_scratchpad_size = member_size(wp_UploadScratchpadReq_scratchpad_t, bytes);

    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_config_handle_set_scratchpad_target_and_action_request(
                                            wp_SetScratchpadTargetAndActionReq *req,
                                            wp_SetScratchpadTargetAndActionResp *resp)
{
    app_res_e res = APP_RES_OK;
    uint8_t seq;
    uint8_t param = 0;
    uint16_t crc;

    // TODO: Add some sanity checks

    switch (req->target_and_action.action) {

        case wp_ScratchpadAction_NO_OTAP :
            res = WPC_write_target_scratchpad(0, 0, 0, 0);
            LOGI("Target and action pushed NoOtap\n", res);
            break;

        case wp_ScratchpadAction_PROPAGATE_AND_PROCESS_WITH_DELAY :
            if (req->target_and_action.which_param == wp_TargetScratchpadAndAction_delay_tag)
            {
                param = convert_delay_to_app_format(req->target_and_action.param.delay);
                if (param == 0)
                {
                    res = APP_RES_INVALID_VALUE;
                    break;
                }
            }
            else if (req->target_and_action.which_param == wp_TargetScratchpadAndAction_raw_tag)
            {
                param = req->target_and_action.param.raw;
            }
            else
            {
                res = APP_RES_INVALID_VALUE;
                break;
            }

        case wp_ScratchpadAction_PROPAGATE_ONLY :
        case wp_ScratchpadAction_PROPAGATE_AND_PROCESS :
            if (req->target_and_action.has_target_sequence)
            {
                seq = req->target_and_action.target_sequence;
            }
            else if (m_sink_config.otap_status.scrat_seq_number != 0)
            {
                seq = m_sink_config.otap_status.scrat_seq_number;
                LOGI("Adding seq from node: %u\n", seq);
            }
            else
            {
                res = APP_RES_INVALID_VALUE;
                break;
            }

            if (req->target_and_action.has_target_crc)
            {
                crc = req->target_and_action.target_crc;
            }
            else
            {
                // No check on CRC as invalid scratchpad is already handled with seq
                crc = m_sink_config.otap_status.scrat_crc;
                LOGI("Adding CRC from node: %u\n", crc);
            }

            res = WPC_write_target_scratchpad(seq,
                                              crc,
                                              req->target_and_action.action - wp_ScratchpadAction_NO_OTAP,
                                              param);

            // Read otap variable back to be sure everything is updated
            initialize_otap_variables();

            break;

        default:
            res = APP_RES_INVALID_VALUE;
    }

    if (res != APP_RES_OK)
    {
        LOGE("Target and action failed %d\n", res);
    }

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(res));

    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_config_get_current_event_status(bool online,
                                                      uint8_t * event_status_p,
                                                      size_t * event_status_size_p)
{
    bool status;
    wp_GenericMessage message = wp_GenericMessage_init_zero;
    wp_WirepasMessage message_wirepas = wp_WirepasMessage_init_zero;
    message.wirepas = &message_wirepas;

    // Allocate the needed space for only the submessage we want to send
    wp_StatusEvent * message_StatusEvent_p = Platform_malloc(sizeof(wp_StatusEvent));
    if (message_StatusEvent_p == NULL)
    {
        LOGE("Not enough memory to encode StatusEvent\n");
        return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
    }

    message_wirepas.status_event = message_StatusEvent_p;

    fill_status_event(message_StatusEvent_p,
                      online ? wp_OnOffState_ON : wp_OnOffState_OFF,
                      online ? 1 : 0);

    // Using the module static buffer
    pb_ostream_t stream = pb_ostream_from_buffer(event_status_p, *event_status_size_p);

    /* Now we are ready to encode the message! */
	status = pb_encode(&stream, wp_GenericMessage_fields, &message);

    /* Release buffer as we don't need it anymore */
    Platform_free(message_StatusEvent_p, sizeof(wp_StatusEvent));

	if (!status) {
		LOGE("StatusEvent encoding failed: %s\n", PB_GET_ERROR(&stream));
        *event_status_size_p = 0;
        return APP_RES_PROTO_CANNOT_GENERATE_RESPONSE;
    }

    LOGI("Msg size %d\n", stream.bytes_written);
    *event_status_size_p = stream.bytes_written;
    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_config_register_for_event_status(onEventStatus_cb_f onProtoEventStatus_cb)
{
    // Only support one "client" for now
    if (m_onProtoEventStatus_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_onProtoEventStatus_cb = onProtoEventStatus_cb;
    return APP_RES_PROTO_OK;
}

net_addr_t Proto_config_get_network_address(void)
{
    // Take it from the cache. It will be called
    // for every rx message so cannot be read from node
    return m_sink_config.network_address;
}

void Proto_config_refresh_otap_infos()
{
    // Force config refresh and send event status
    onStackStatusReceived(IGNORE_STATUS);
}