/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include "proto_config.h"
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
} sink_config_t;

/* TODO : Protect config access */

/** Sink config storage */
static sink_config_t m_sink_config;

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

static void convert_role_to_proto_format(app_role_t role, wp_NodeRole * proto_role)
{
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

static bool initialize_config_variables()
{
    _Static_assert(member_size(wp_NodeRole, flags) == (2 * sizeof(wp_NodeRole_RoleFlags)) );
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

    if (!res)
    {
        LOGE("All the settings cannot be read\n");
    }

    return res;
}

static void fill_sink_read_config(wp_SinkReadConfig * config_p)
{
    _Static_assert( member_size(wp_AppConfigData, app_config_data)
                    == member_size(msap_app_config_data_write_req_pl_t, app_config_data));

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
        .has_stored_scratchpad = false,
        .has_stored_status = false,
        .has_stored_type = false,
        .has_processed_scratchpad = false,
        .has_firmware_area_id = false,
        .has_target_and_action = false
    };

    strcpy(config_p->sink_id, Common_get_sink_id());

    convert_role_to_proto_format(m_sink_config.app_node_role, &config_p->node_role);

    if (MAXIMUM_APP_CONFIG_SIZE >= m_sink_config.app_config_max_size)
    {
        uint16_t diag_data_interval = 0;  // needed to avoid trouble with pointer alignement
        uint8_t seq = 0;

        if (WPC_get_app_config_data(&seq,
                                    &diag_data_interval,
                                    config_p->app_config.app_config_data,
                                    m_sink_config.app_config_max_size)
            != APP_RES_OK)
        {
            LOGE("Cannot get App config data from node\n");
            config_p->has_app_config = false;
        }
        config_p->app_config.seq = seq;
        config_p->app_config.diag_interval_s = diag_data_interval;
    }
}

static void onStackStatusReceived(uint8_t status)
{
    bool res;
    wp_StatusEvent * message_StatusEvent_p;
    uint8_t * encoded_message_p;
    wp_GenericMessage message = wp_GenericMessage_init_zero;
    wp_WirepasMessage message_wirepas = wp_WirepasMessage_init_zero;
    message.wirepas = &message_wirepas;

    LOGI("Status received : %d\n", status);

    Proto_config_on_stack_boot_status(status);

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

    *message_StatusEvent_p = (wp_StatusEvent){
        .version = GW_PROTO_MESSAGE_VERSION,
        .state = wp_OnOffState_ON, // gateway state, always ONLINE
        .configs_count = 1,
    };

    message_StatusEvent_p->has_gw_model = (strlen(Common_get_gateway_model()) != 0);
    strcpy(message_StatusEvent_p->gw_model, Common_get_gateway_model());
    message_StatusEvent_p->has_gw_version = (strlen(Common_get_gateway_version()) != 0);
    strcpy(message_StatusEvent_p->gw_version, Common_get_gateway_version());

    fill_sink_read_config(&message_StatusEvent_p->configs[0]);

    Common_fill_event_header(&message_StatusEvent_p->header);

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
    initialize_config_variables();

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


void Proto_config_on_stack_boot_status(uint8_t status)
{
    m_sink_config.StackStatus = status;

    if ((status & APP_STACK_STOPPED) == 0)
    {
        LOGI("Stack started\n");
    }

    // After a reboot, read again the variables
    initialize_config_variables();
}

app_proto_res_e Proto_config_handle_set_config(wp_SetConfigReq *req,
                                               wp_SetConfigResp *resp)
{
    return APP_RES_PROTO_NOT_IMPLEMENTED;
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
                                Common_convert_error_code(APP_RES_OK));
    resp->configs_count = 1;
    fill_sink_read_config(&resp->configs[0]);

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

    *message_StatusEvent_p = (wp_StatusEvent){
        .version = GW_PROTO_MESSAGE_VERSION,
        .configs_count = 0,
    };

    if (online)
    {
        // Generate status event with state ONLINE
        message_StatusEvent_p->state = wp_OnOffState_ON;

        // Add current config for online node
        message_StatusEvent_p->configs_count = 1;
        fill_sink_read_config(&message_StatusEvent_p->configs[0]);
    }
    else
    {
        // Generate status event with state OFFLINE
        message_StatusEvent_p->state = wp_OnOffState_OFF;
    }

    message_StatusEvent_p->has_gw_model = (strlen(Common_get_gateway_model()) != 0);
    strcpy(message_StatusEvent_p->gw_model, Common_get_gateway_model());
    message_StatusEvent_p->has_gw_version = (strlen(Common_get_gateway_version()) != 0);
    strcpy(message_StatusEvent_p->gw_version, Common_get_gateway_version());

    Common_fill_event_header(&message_StatusEvent_p->header);

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
