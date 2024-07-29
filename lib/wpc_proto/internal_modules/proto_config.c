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


/** Structure to hold config from node */
typedef struct sink_config
{
    uint16_t stack_profile;
    uint16_t hw_magic;
    uint16_t ac_limit_min;
    uint16_t ac_limit_max;
    uint16_t app_config_max_size;
    uint16_t version[4];
    uint8_t max_mtu;
    uint8_t ch_range_min;
    uint8_t ch_range_max;
    uint8_t pdu_buffer_size;

    bool CipherKeySet;
    bool AuthenticationKeySet;
    uint8_t StackStatus;
    uint16_t ac_range_min_cur;  // 0 means unset ?
    uint16_t ac_range_max_cur;

    app_role_t node_address;
    app_role_t app_node_role;
    wp_NodeRole wp_node_role;  // same as app_node_role, different format
    net_addr_t network_address;
    net_channel_t network_channel;

    msap_app_config_data_write_req_pl_t app_config;
} sink_config_t;

/* TODO : Protect config access */

/** Sink config storage */
sink_config_t m_sink_config;

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
    res &= get_value_from_node(WPC_get_channel_limits, &m_sink_config.ch_range_min, &m_sink_config.ch_range_max,
                               "Channel Range", false);
    res &= get_value_from_node(WPC_get_access_cycle_limits, &m_sink_config.ac_limit_min, &m_sink_config.ac_limit_max,
                               "AC Range", false);
    res &= get_value_from_node(WPC_get_app_config_data_size, &m_sink_config.app_config_max_size, NULL,
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

    res &= get_value_from_node(WPC_is_cipher_key_set, &m_sink_config.CipherKeySet, NULL,
                               "Cipher key set", false);
    res &= get_value_from_node(WPC_is_authentication_key_set, &m_sink_config.AuthenticationKeySet, NULL,
                               "Authentication key set", false);
    res &= get_value_from_node(WPC_get_stack_status, &m_sink_config.StackStatus, NULL, "Stack Status", false);

    res &= get_value_from_node(WPC_get_access_cycle_range, &m_sink_config.ac_range_min_cur, &m_sink_config.ac_range_max_cur,
                               "Current access cycle range", false);

    res &= get_value_from_node(WPC_get_node_address, &m_sink_config.node_address, NULL,
                               "Network address", m_sink_config.StackStatus & APP_STACK_NODE_ADDRESS_NOT_SET);

    res &= get_value_from_node(WPC_get_role, &role, NULL,
                               "Node role", m_sink_config.StackStatus & APP_STACK_ROLE_NOT_SET);
    m_sink_config.app_node_role = role;
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

    res &= get_value_from_node(WPC_get_network_address, &m_sink_config.network_address, NULL,
                               "Network address", m_sink_config.StackStatus & APP_STACK_NETWORK_ADDRESS_NOT_SET);

    res &= get_value_from_node(WPC_get_network_channel, &m_sink_config.network_channel, NULL,
                               "Network channel", m_sink_config.StackStatus & APP_STACK_NETWORK_CHANNEL_NOT_SET);

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


void Proto_config_fill_config(wp_SinkReadConfig * config_p)
{
    _Static_assert( member_size(wp_AppConfigData, app_config_data)
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
    memcpy(config_p->app_config.app_config_data,
           m_sink_config.app_config.app_config_data, MAXIMUM_APP_CONFIG_SIZE);

    strcpy(config_p->sink_id, Common_get_sink_id());
}

bool Proto_config_init(void)
{
    /* Read initial config from sink */
    initialize_unmodifiable_variables();
    initialize_variables();
    return true;
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
        LOGE("Get stack status failed\n");
    }

    LOGE("WPC_get_config res=%d\n", res);

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(res));
    resp->configs_count = 1;
    Proto_config_fill_config(&resp->configs[0]);

    return APP_RES_PROTO_OK;
}