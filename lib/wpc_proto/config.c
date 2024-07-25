/* Copyright 2019 Wirepas Ltd licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "platform.h"
#include "wpc_types.h"
#include "wpc_proto.h"
#include "config.h"
#include "config_message.pb.h"

#define LOG_MODULE_NAME "Config"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "config_message.pb.h"

/* Set the max of strings used (including \0), matching values defined in proto
 * files */
#define GATEWAY_ID_MAX_SIZE      16
#define SINK_ID_MAX_SIZE         16

/** Structure to hold config from node */
typedef struct sink_config
{
    /* Read only parameters backup-ed with a table (Read at boot up) */
    uint16_t stack_profile;
    uint16_t hw_magic;
    uint16_t ac_limit_min;
    uint16_t ac_limit_max;
    uint16_t app_config_max_size;
    uint16_t version[4];
    uint8_t  max_mtu;
    uint8_t  ch_range_min;
    uint8_t  ch_range_max;
    uint8_t  pdu_buffer_size;

    /* Read parameters with node interrogation */
    //uint16_t CurrentAC;
    bool     CipherKeySet;
    bool     AuthenticationKeySet;
    uint8_t  StackStatus;

    /* Read write also ? */
    uint16_t ac_range_min_cur;  // 0 means unset ?
    uint16_t ac_range_max_cur;

    /* Read/Write parameters with node interrogation */
    app_role_t    node_address;
    app_role_t    app_node_role;
    wp_NodeRole   wp_node_role; // same as app_node_role, different format
    net_addr_t    network_address;
    net_channel_t network_channel;
    // uint32_t channel_map; // deprecated

    /* Methods related to config */
    msap_app_config_data_write_req_pl_t app_config;
} sink_config_t;

/* TODO : check how to get access to config cache. Protected access ? */
/** Sink values read at init time */
sink_config_t m_sink_config;

static char     m_gateway_id[GATEWAY_ID_MAX_SIZE];
static char     m_sink_id[SINK_ID_MAX_SIZE];
static uint32_t m_event_id = 0;

#if 0 // temporary deactivate of read/write access

/**
 * \brief   Global function to set a key (cipher or authen)
 * \param   value
 *          The message received on dbus containing the key
 * \param   key_set_function
 *          The WPC key function to use
 * \return  Return code of operation
 */
static app_res_e set_key(sd_bus_message * value, set_key_f key_set_function)
{
    const void * key;
    app_res_e res = APP_RES_INTERNAL_ERROR;
    size_t n;
    int r;

    r = sd_bus_message_read_array(value, 'y', &key, &n);
    if ((r < 0) || (n != 16))
    {
        LOGE("Cannot get key from request %s len=%d\n", strerror(-r), n);
        return res;
    }

    res = (key_set_function)((uint8_t *) key);
    return res;
}

/**
 * \brief   Set cipher key handler
 * \param   ... (from sd_bus property handler)
 */
static int set_cipher_key(sd_bus * bus,
                          const char * path,
                          const char * interface,
                          const char * property,
                          sd_bus_message * value,
                          void * userdata,
                          sd_bus_error * error)
{
    app_res_e res;

    res = set_key(value, WPC_set_cipher_key);
    if (res != APP_RES_OK)
    {
        SET_WPC_ERROR(error, "WPC_set_cipher_key", res);
        return -EINVAL;
    }

    return 0;
}

/**
 * \brief   Set authentication key handler
 * \param   ... (from sd_bus property handler)
 */
static int set_authen_key(sd_bus * bus,
                          const char * path,
                          const char * interface,
                          const char * property,
                          sd_bus_message * value,
                          void * userdata,
                          sd_bus_error * error)
{
    app_res_e res;
    res = set_key(value, WPC_set_authentication_key);
    if (res != APP_RES_OK)
    {
        SET_WPC_ERROR(error, "WPC_set_authentication_key", res);
        return -EINVAL;
    }

    return 0;
}


/**
 * \brief   Set stack state
 * \param   ... (from sd_bus function signature)
 */
static int set_stack_state(sd_bus_message * m, void * userdata, sd_bus_error * error)
{
    app_res_e res;
    bool state;
    int r;

    /* Read the parameters */
    r = sd_bus_message_read(m, "b", &state);
    if (r < 0)
    {
        LOGE("Fail to parse parameters: %s\n", strerror(-r));
        sd_bus_error_set_errno(error, EINVAL);
        return r;
    }

    if (state)
    {
        res = WPC_start_stack();
        WPC_set_autostart(1);
        if (res == APP_RES_OK)
        {
            LOGI("Stack started manually\n");
            send_dbus_signal("StackStarted");
        }
    }
    else
    {
        WPC_set_autostart(0);
        res = WPC_stop_stack();
        if (res == APP_RES_OK)
        {
            LOGI("Stack stopped manually\n");
            send_dbus_signal("StackStopped");
        }
    }

    if (res != APP_RES_OK)
    {
        LOGE("Set stack state (%d) res = %d\n", state, res);
    }

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "b", res == APP_RES_OK);
}

/**
 * \brief   Clear cipher key
 * \param   ... (from sd_bus function signature)
 */
static int clear_cipher_key(sd_bus_message * m, void * userdata, sd_bus_error * error)
{
    app_res_e res = WPC_remove_cipher_key();
    if (res != APP_RES_OK)
    {
        SET_WPC_ERROR(error, "WPC_remove_cipher_key", res);
        return -EINVAL;
    }
    // No return code
    return sd_bus_reply_method_return(m, "");
}

/**
 * \brief   Clear authentication key
 * \param   ... (from sd_bus function signature)
 */
static int clear_authen_key(sd_bus_message * m, void * userdata, sd_bus_error * error)
{
    app_res_e res = WPC_remove_authentication_key();
    if (res != APP_RES_OK)
    {
        SET_WPC_ERROR(error, "WPC_remove_authentication_key", res);
        return -EINVAL;
    }

    // No return code
    return sd_bus_reply_method_return(m, "");
}

/** \brief  Maximum reserved size for app config */
#define MAX_APP_CONFIG_SIZE 128

/**
 * \brief   Get app config handler
 * \param   ... (from sd_bus function signature)
 */
static int get_app_config(sd_bus_message * m, void * userdata, sd_bus_error * error)
{
    int r;
    app_res_e res;
    uint8_t seq;
    uint16_t interval;
    uint8_t app_config[MAX_APP_CONFIG_SIZE];
    uint8_t size;

    sd_bus_message * reply = NULL;

    res = WPC_get_app_config_data_size(&size);
    if (res != APP_RES_OK)
    {
        LOGE("Cannot determine app config size\n");
        SET_WPC_ERROR(error, "WPC_get_app_config_data_size", res);
        return -EINVAL;
    }

    if (size > MAX_APP_CONFIG_SIZE)
    {
        LOGE("App config size too big compared to reserved buffer\n");
        sd_bus_error_set_errno(error, ENOMEM);
        return -ENOMEM;
    }

    res = WPC_get_app_config_data(&seq, &interval, app_config, size);
    if (res != APP_RES_OK)
    {
        if (res != APP_RES_NO_CONFIG)
        {
            LOGE("Cannot get app config %d\n", res);
        }
        SET_WPC_ERROR(error, "WPC_get_app_config_data", res);
        return -EINVAL;
    }

    /* Create the answer */
    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
    {
        sd_bus_error_set_errno(error, r);
        LOGE("Cannot create new message return %s\n", strerror(-r));
        return r;
    }

    r = sd_bus_message_append(reply, "yq", seq, interval);
    if (r < 0)
    {
        sd_bus_error_set_errno(error, r);
        LOGE("Cannot append parameters: %s\n", strerror(-r));
        return r;
    }

    r = sd_bus_message_append_array(reply, 'y', app_config, size);
    if (r < 0)
    {
        sd_bus_error_set_errno(error, r);
        LOGE("Cannot append array: %s\n", strerror(-r));
        return r;
    }

    return sd_bus_send(NULL, reply, NULL);
}

/**
 * \brief   Set app config handler
 * \param   ... (from sd_bus function signature)
 */
static int set_app_config(sd_bus_message * m, void * userdata, sd_bus_error * error)
{
    uint8_t seq;
    uint16_t interval;
    const void * app_config;
    size_t n;
    int r;
    app_res_e res;

    /* Read the parameters */
    r = sd_bus_message_read(m, "yq", &seq, &interval);
    if (r < 0)
    {
        sd_bus_error_set_errno(error, r);
        LOGE("Fail to parse parameters: %s\n", strerror(-r));
        return r;
    }

    r = sd_bus_message_read_array(m, 'y', &app_config, &n);
    if (r < 0)
    {
        sd_bus_error_set_errno(error, r);
        LOGE("Fail to parse app config bytes: %s\n", strerror(-r));
        return r;
    }

    res = WPC_set_app_config_data(seq, interval, (uint8_t *) app_config, n);
    if (res != APP_RES_OK)
    {
        SET_WPC_ERROR(error, "WPC_set_app_config_data", res);
        return -EINVAL;
    }

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "b", true);
}

#endif

/**
 * \brief   Apply new config if any param has changed
 * \param   req
 *          pointer to the request received
 * \param   resp
 *          Pointer to the reponse to send back
 * \return  APP_RES_PROTO_OK if answer is ready to send
 */

app_proto_res_e Config_Handle_set_config_request(wp_SetConfigReq *req,
                                                 wp_SetConfigResp *resp)
{
    app_res_e res                = APP_RES_OK;
    bool      config_has_changed = false;

    // TODO: Add some sanity checks
    // res = WPC_send_data(req->payload.bytes,
    //                     req->payload.size,
    //                     0,
    //                     req->destination_address,
    //                     req->qos,
    //                     (uint8_t) req->source_endpoint,
    //                     (uint8_t) req->destination_endpoint,
    //                     NULL,
    //                     0); // No initial delay supported

    // check any config change and apply them

    // req->header. 
    //     /* Unique request id */
    //     uint64_t req_id;
    //     /* Sink id if relevant for request */
    //     bool has_sink_id;
    //     char sink_id[16];
    //     /* Timestamp for the request generation */
    //     bool has_time_ms_epoch;
    //     uint64_t time_ms_epoch;

    // check new config, and new parameters
    wp_SinkNewConfig * cfg = &req->config;

    //cfg->sink_id[16]; not used

    if (cfg->has_node_role)
    {
        app_role_t options = 0;
        if (cfg->node_role.flags_count > 0)
        {
            options |= (cfg->node_role.flags[0] == wp_NodeRole_RoleFlags_LOW_LATENCY)
                       ? APP_ROLE_OPTION_LL : 0;
            options |= (cfg->node_role.flags[0] == wp_NodeRole_RoleFlags_AUTOROLE)
                       ? APP_ROLE_OPTION_AUTOROLE : 0;
        }
        if (cfg->node_role.flags_count > 1)
        {
            options |= (cfg->node_role.flags[1] == wp_NodeRole_RoleFlags_LOW_LATENCY)
                       ? APP_ROLE_OPTION_LL : 0;
            options |= (cfg->node_role.flags[1] == wp_NodeRole_RoleFlags_AUTOROLE)
                       ? APP_ROLE_OPTION_AUTOROLE : 0;
        }
        app_role_t new_role = CREATE_ROLE(cfg->node_role.role, options);
        if (   (new_role != m_sink_config.app_node_role)
            || (m_sink_config.StackStatus & APP_STACK_ROLE_NOT_SET) )
        {
            res = WPC_set_role(new_role);
            if (res != APP_RES_OK)
            {
                goto exit_on_error;
            }
            config_has_changed = true;
        }
    }

        // bool has_node_address;
            // uint32_t node_address;
        // bool has_network_address;
            // uint64_t network_address;
        // bool has_network_channel;
            // uint32_t network_channel;
        // bool has_app_config;
            // wp_AppConfigData app_config;         -> set_app_config
        // bool has_keys;
            // wp_NetworkKeys keys;                 -> set_key
        // wp_AccessCycleRange current_ac_range;
        // bool has_sink_state;
            // wp_OnOffState sink_state;            -> set_stack_state

exit_on_error:
    // if any config changed, send status event
    if (config_has_changed)
    {
        //send event status
    }

    LOGI("WPC_set_config res=%d\n", res);

    Config_Fill_response_header(&resp->header,
                                req->header.req_id,
                                convert_error_code(APP_ERROR_CODE_LUT, res));
    Config_Fill_config(&resp->config);

    return APP_RES_PROTO_OK;
}

bool Config_Get_has_network_address()
{
    return (m_sink_config.StackStatus & APP_STACK_NETWORK_ADDRESS_NOT_SET)
               ? false
               : true;
}

net_addr_t Config_Get_network_address()
{
    return m_sink_config.network_address;
}

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
 * \param   Not_condition
 *          if true, values are not read, no error returned
 * \return  True if correctly read, false otherwise
 */
static bool get_value_from_node(void * f,
                                void * var1,
                                void * var2,
                                char * var_name,
                                bool Not_condition)
{
    app_res_e res = APP_RES_INTERNAL_ERROR;
    if (Not_condition)
    {
        // Values are not defined
        return true;
    }
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

static bool initialize_unmodifiable_variables()
{
    bool res = true;

    res &= get_value_from_node(WPC_get_stack_profile, &m_sink_config.stack_profile, NULL, "Stack profile", false);
    res &= get_value_from_node(WPC_get_hw_magic, &m_sink_config.hw_magic, NULL, "Hw magic", false);
    res &= get_value_from_node(WPC_get_mtu, &m_sink_config.max_mtu, NULL, "MTU", false);
    res &= get_value_from_node(WPC_get_pdu_buffer_size, &m_sink_config.pdu_buffer_size, NULL, "PDU Buffer Size", false);
    res &= get_value_from_node(WPC_get_channel_limits,
                               &m_sink_config.ch_range_min,
                               &m_sink_config.ch_range_max,
                               "Channel Range", false);
    res &= get_value_from_node(WPC_get_access_cycle_limits,
                               &m_sink_config.ac_limit_min,
                               &m_sink_config.ac_limit_max,
                               "AC Range", false);
    res &= get_value_from_node(WPC_get_app_config_data_size,
                               &m_sink_config.app_config_max_size,
                               NULL,
                               "App Config Max size", false);

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
        LOGE("All the static settings cannot be read\n");
    }

    return res;
}

static bool initialize_variables()
{
    _Static_assert(member_size(wp_NodeRole, flags) == (2 * sizeof(wp_NodeRole_RoleFlags)) );

    bool res = true;
    app_role_t role = 0;
    pb_size_t flags_count = 0;

    //res &= get_value_from_node(WPC_get_current_access_cycle, &m_sink_config.CurrentAC, NULL, "Current Access Cycle", false);
    res &= get_value_from_node(WPC_is_cipher_key_set, &m_sink_config.CipherKeySet, NULL, "Cipher key set", false);
    res &= get_value_from_node(WPC_is_authentication_key_set, &m_sink_config.AuthenticationKeySet, NULL, "Authentication key set", false);
    res &= get_value_from_node(WPC_get_stack_status, &m_sink_config.StackStatus, NULL, "Stack Status", false);

    res &= get_value_from_node(WPC_get_access_cycle_range,
                               &m_sink_config.ac_range_min_cur,
                               &m_sink_config.ac_range_max_cur,
                               "Current access cycle range", false);

    res &= get_value_from_node(WPC_get_node_address,
                               &m_sink_config.node_address,
                               NULL,
                               "Network address",
                               m_sink_config.StackStatus & APP_STACK_NODE_ADDRESS_NOT_SET);

    res &= get_value_from_node(WPC_get_role,
                               &role,
                               NULL,
                               "Node role",
                               m_sink_config.StackStatus & APP_STACK_ROLE_NOT_SET);
    m_sink_config.app_node_role  = role;
    m_sink_config.wp_node_role.role = GET_BASE_ROLE(role);
    if (GET_ROLE_OPTIONS(role) & APP_ROLE_OPTION_LL)
    {
        m_sink_config.wp_node_role.flags[flags_count] = wp_NodeRole_RoleFlags_LOW_LATENCY;
        flags_count++;
    }
    if (GET_ROLE_OPTIONS(role) & APP_ROLE_OPTION_AUTOROLE)
    {
        m_sink_config.wp_node_role.flags[flags_count] = wp_NodeRole_RoleFlags_AUTOROLE;
        flags_count++;
    }
    m_sink_config.wp_node_role.flags_count = flags_count;

    res &= get_value_from_node(WPC_get_network_address,
                               &m_sink_config.network_address,
                               NULL,
                               "Network address",
                               m_sink_config.StackStatus & APP_STACK_NETWORK_ADDRESS_NOT_SET);

    res &= get_value_from_node(WPC_get_network_channel,
                               &m_sink_config.network_channel,
                               NULL,
                               "Network channel",
                               m_sink_config.StackStatus & APP_STACK_NETWORK_CHANNEL_NOT_SET);

    if ( !(m_sink_config.StackStatus & APP_STACK_APP_DATA_NOT_SET))
    {
        uint16_t diag_data_interval = 0 ; // needed to avoid trouble with pointer alignement
        if (WPC_get_app_config_data(&m_sink_config.app_config.sequence_number,
                                    &diag_data_interval,
                                    m_sink_config.app_config.app_config_data,
                                    m_sink_config.app_config_max_size)
            != APP_RES_OK)
        {
            LOGE("Cannot get App config data from node\n");
            res = false;
        }
        m_sink_config.app_config.diag_data_interval = diag_data_interval;
    }

    if (!res)
    {
        LOGE("All the variable settings cannot be read\n");
    }

    return res;
}

void Config_On_stack_boot_status(uint8_t status)
{
    m_sink_config.StackStatus = status;

    if ((status & APP_STACK_STOPPED) == 0)
    {
        LOGI("Stack started\n");
    }
    
    // After a reboot, read again the variables
    initialize_unmodifiable_variables();
    initialize_variables();
}

void Config_Fill_event_header(wp_EventHeader * header_p)
{
    _Static_assert(member_size(wp_EventHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_EventHeader, sink_id) >= SINK_ID_MAX_SIZE);

    strncpy(header_p->gw_id, m_gateway_id, GATEWAY_ID_MAX_SIZE);
    header_p->gw_id[GATEWAY_ID_MAX_SIZE - 1] = '\0';
    strncpy(header_p->sink_id, m_sink_id, SINK_ID_MAX_SIZE);
    header_p->sink_id[SINK_ID_MAX_SIZE - 1] = '\0';
    header_p->has_sink_id            = (strlen(m_sink_id) != 0);
    header_p->has_time_ms_epoch      = true;
    header_p->time_ms_epoch          = Platform_get_timestamp_ms_epoch();
    header_p->event_id               = m_event_id++;
}


void Config_Fill_response_header(wp_ResponseHeader * header_p, uint64_t req_id, wp_ErrorCode res)
{
    _Static_assert(member_size(wp_ResponseHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_ResponseHeader, sink_id) >= SINK_ID_MAX_SIZE);

    strncpy(header_p->gw_id, m_gateway_id, GATEWAY_ID_MAX_SIZE);
    header_p->gw_id[GATEWAY_ID_MAX_SIZE - 1] = '\0';
    strncpy(header_p->sink_id, m_sink_id, SINK_ID_MAX_SIZE);
    header_p->sink_id[SINK_ID_MAX_SIZE - 1] = '\0';
    header_p->has_sink_id                   = (strlen(m_sink_id) != 0);
    header_p->has_time_ms_epoch             = true;
    header_p->time_ms_epoch                 = Platform_get_timestamp_ms_epoch();
    header_p->req_id                        = req_id;
    header_p->res                           = res;
}

void Config_Fill_config(wp_SinkReadConfig * config_p)
{
    _Static_assert(
        member_size(wp_AppConfigData, app_config_data)
        == member_size(msap_app_config_data_write_req_pl_t, app_config_data));

    uint8_t status = m_sink_config.StackStatus;

    *config_p = (wp_SinkReadConfig){
        /* Sink minimal config */
        .has_node_role = ((status & APP_STACK_ROLE_NOT_SET) ? false : true),
        .node_role = m_sink_config.wp_node_role,
        .has_node_address = ((status & APP_STACK_NODE_ADDRESS_NOT_SET) ? false : true),
        .node_address = m_sink_config.node_address,
        .has_network_address = ((status & APP_STACK_NETWORK_ADDRESS_NOT_SET) ? false : true),
        .network_address = m_sink_config.network_address,
        .has_network_channel = ((status & APP_STACK_NETWORK_CHANNEL_NOT_SET) ? false : true),
        .network_channel = m_sink_config.network_channel,
        .has_app_config = ((status & APP_STACK_APP_DATA_NOT_SET) ? false : true),
        .app_config = { .diag_interval_s = m_sink_config.app_config.diag_data_interval,
                        .seq = m_sink_config.app_config.sequence_number},                         
        .has_channel_map = false,                                       
        .has_are_keys_set = true,                                       
        .are_keys_set =    m_sink_config.CipherKeySet 
                        && m_sink_config.AuthenticationKeySet,          

        .has_current_ac_range = (m_sink_config.ac_range_min_cur == 0
                                     ? false
                                     : true),                           
        
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
                             .dev =   m_sink_config.version[3] },       
        /* State of sink */
        .has_sink_state = true,                                         
        .sink_state = ((status & APP_STACK_STOPPED) ?
                            wp_OnOffState_OFF : wp_OnOffState_ON),      
        /* Scratchpad info for the sink */
        .has_stored_scratchpad = false,                                 
        .has_stored_status = false,                                     
        .has_stored_type = false,                                       
        .has_processed_scratchpad = false,                              
        .has_firmware_area_id = false,                                  
        .has_target_and_action = false                                  
    };
    memcpy(config_p->app_config.app_config_data,
           m_sink_config.app_config.app_config_data, MAXIMUM_APP_CONFIG_SIZE);

    strncpy(config_p->sink_id,
            m_sink_id,
            member_size(wp_SinkReadConfig, sink_id));
    config_p->sink_id[member_size(wp_SinkReadConfig, sink_id) - 1] = '\0';
}


int Config_Init(char * gateway_id,
                char * sink_id)
{
    //int r;
    uint8_t status;

    strncpy(m_gateway_id, gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE - 1] = '\0';

    strncpy(m_sink_id, sink_id, SINK_ID_MAX_SIZE);
    m_sink_id[SINK_ID_MAX_SIZE - 1] = '\0';

    LOGI("WPC config initialize with gw_id %s, sink_id %s\n",
                m_gateway_id,
                m_sink_id);

    /* Read unmodifiable config from sink */
    initialize_unmodifiable_variables();
    initialize_variables();

    if (WPC_get_stack_status(&status) == APP_RES_OK)
    {
        m_sink_config.StackStatus = status;
        LOGI("Stack is %s\n", status == 0 ? "started" : "stopped");
    }
    else
    {
        LOGE("Cannot determine stack state\n");
    }

    LOGI("Config init done\n");
    return 0;
}

void Config_Close()
{
    WPC_unregister_from_stack_status();
}
