/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdlib.h>

#include "common.h"
#include "platform.h"
#include "wp_global.pb.h"

// String filled during init, ensuring that there're null terminated
static char m_gateway_id[GATEWAY_ID_MAX_SIZE];
static char m_gateway_model[GATEWAY_MODEL_MAX_SIZE];
static char m_gateway_version[GATEWAY_VERSION_MAX_SIZE];
static char m_sink_id[SINK_ID_MAX_SIZE];

/* Error code LUT for protobuf errors from app_res_e */
static const wp_ErrorCode APP_ERROR_CODE_LUT[] = {
    wp_ErrorCode_OK                             ,   // APP_RES_OK,                           Everything is ok
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_NOT_STOPPED,            Stack is not stopped
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_ALREADY_STOPPED,        Stack is already stopped
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_ALREADY_STARTED,        Stack is already started
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_VALUE,                A parameter has an invalid value
    wp_ErrorCode_INVALID_ROLE                   ,   // APP_RES_ROLE_NOT_SET,                 The node role is not set
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_NODE_ADD_NOT_SET,             The node address is not set
    wp_ErrorCode_INVALID_NETWORK_ADDRESS        ,   // APP_RES_NET_ADD_NOT_SET,              The network address is not set
    wp_ErrorCode_INVALID_NETWORK_CHANNEL        ,   // APP_RES_NET_CHAN_NOT_SET,             The network channel is not set
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_IS_STOPPED,             Stack is stopped
    wp_ErrorCode_INVALID_ROLE                   ,   // APP_RES_NODE_NOT_A_SINK,              Node is not a sink
    wp_ErrorCode_INVALID_DEST_ADDRESS           ,   // APP_RES_UNKNOWN_DEST,                 Unknown destination address
    wp_ErrorCode_INVALID_APP_CONFIG             ,   // APP_RES_NO_CONFIG,                    No configuration received/set
    wp_ErrorCode_INTERNAL_ERROR                 ,   // APP_RES_ALREADY_REGISTERED,           Cannot register several times
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_NOT_REGISTERED,               Cannot unregister if not registered first
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_ATTRIBUTE_NOT_SET,            Attribute is not set yet
    wp_ErrorCode_ACCESS_DENIED                  ,   // APP_RES_ACCESS_DENIED,                Access denied
    wp_ErrorCode_INVALID_DATA_PAYLOAD           ,   // APP_RES_DATA_ERROR,                   Error in data
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_NO_SCRATCHPAD_START,          No scratchpad start request sent
    wp_ErrorCode_INVALID_SCRATCHPAD             ,   // APP_RES_NO_VALID_SCRATCHPAD,          No valid scratchpad
    wp_ErrorCode_INVALID_ROLE                   ,   // APP_RES_NOT_A_SINK,                   Stack is not a sink
    wp_ErrorCode_SINK_OUT_OF_MEMORY             ,   // APP_RES_OUT_OF_MEMORY,                Out of memory
    wp_ErrorCode_INVALID_DIAG_INTERVAL          ,   // APP_RES_INVALID_DIAG_INTERVAL,        Invalid diag interval
    wp_ErrorCode_INVALID_SEQUENCE_NUMBER        ,   // APP_RES_INVALID_SEQ,                  Invalid sequence number
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_START_ADDRESS,        Start address is invalid
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_NUMBER_OF_BYTES,      Invalid number of bytes
    wp_ErrorCode_INVALID_SCRATCHPAD             ,   // APP_RES_INVALID_SCRATCHPAD,           Scratchpad is not valid
    wp_ErrorCode_INVALID_REBOOT_DELAY           ,   // APP_RES_INVALID_REBOOT_DELAY,         Invalid reboot delay
    wp_ErrorCode_INTERNAL_ERROR                     // APP_RES_INTERNAL_ERROR                WPC internal error
};

/**
 * \brief   Intialize the common module
 *          Enforce null termination for strings stored
 * \return  True if successful, False otherwise
 */
bool Common_init(char * gateway_id,
                 char * gateway_model,
                 char * gateway_version,
                 char * sink_id)
{
    strncpy(m_gateway_id, gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE - 1]='\0';
    
    strncpy(m_gateway_model, gateway_model, GATEWAY_MODEL_MAX_SIZE);
    m_gateway_model[GATEWAY_MODEL_MAX_SIZE - 1] = '\0';

    strncpy(m_gateway_version, gateway_version, GATEWAY_VERSION_MAX_SIZE);
    m_gateway_version[GATEWAY_VERSION_MAX_SIZE - 1] = '\0';

    strncpy(m_sink_id, sink_id, SINK_ID_MAX_SIZE);
    m_sink_id[SINK_ID_MAX_SIZE - 1]='\0';

    return true;
}

char * Common_get_gateway_id(void)
{
    return m_gateway_id;
}

char * Common_get_gateway_model(void)
{
    return m_gateway_model;
}

char * Common_get_gateway_version(void)
{
    return m_gateway_version;
}

char * Common_get_sink_id(void)
{
    return m_sink_id;
}

wp_ErrorCode Common_convert_error_code(app_res_e error)
{
    wp_ErrorCode ret = wp_ErrorCode_INTERNAL_ERROR;
    if (error >= 0 && error < sizeof(APP_ERROR_CODE_LUT))
    {
        ret = APP_ERROR_CODE_LUT[error];
    }
    return ret;
}

void Common_fill_event_header(wp_EventHeader * header_p)
{
    _Static_assert(member_size(wp_EventHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_EventHeader, sink_id) >= SINK_ID_MAX_SIZE);

    *header_p = (wp_EventHeader){
        .has_sink_id = (strlen(m_sink_id) != 0),
        .has_time_ms_epoch = true,
        .time_ms_epoch = Platform_get_timestamp_ms_epoch(),
        .event_id = rand() + (((uint64_t) rand()) << 32),
    };

    strncpy(header_p->gw_id, Common_get_gateway_id(), GATEWAY_ID_MAX_SIZE);
    strncpy(header_p->sink_id, Common_get_sink_id(), SINK_ID_MAX_SIZE);
}

void Common_Fill_response_header(wp_ResponseHeader * header_p,
                                 uint64_t req_id,
                                 wp_ErrorCode res)
{
    _Static_assert(member_size(wp_ResponseHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_ResponseHeader, sink_id) >= SINK_ID_MAX_SIZE);

    *header_p = (wp_ResponseHeader) {
        .req_id = req_id,
        .has_sink_id = (strlen(m_sink_id) != 0),
        .res = Common_convert_error_code(res),
        .has_time_ms_epoch = true,
        .time_ms_epoch = Platform_get_timestamp_ms_epoch(),
    };

    strncpy(header_p->gw_id, Common_get_gateway_id(), GATEWAY_ID_MAX_SIZE);
    strncpy(header_p->sink_id, Common_get_sink_id(), SINK_ID_MAX_SIZE);
}

