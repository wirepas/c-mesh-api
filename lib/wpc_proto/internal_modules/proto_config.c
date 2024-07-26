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
    return APP_RES_PROTO_NOT_IMPLEMENTED;
}