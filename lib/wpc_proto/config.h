/* Copyright 2019 Wirepas Ltd licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include "wpc.h"
#include "wp_global.pb.h"
#include "generic_message.pb.h"

/* Return the size of a struct member */
#define member_size(type, member) (sizeof( ((type *)0)->member ))


/**
 * \brief   Macro to convert dual_mcu return code
 *          to library error code based on a LUT
 *          Dual mcu return code are not harmonized so
 *          a different LUT must be used per fonction
 */
#define convert_error_code(LUT, error)                                         \
    ({                                                                         \
        wp_ErrorCode ret = wp_ErrorCode_INTERNAL_ERROR;                        \
        if (error >= 0 && error < sizeof(LUT))                                 \
        {                                                                      \
            ret = LUT[error];                                                  \
        }                                                                      \
        ret;                                                                   \
    })

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
 * \brief       Handle a set config message received
 * \param[in]   req
 *              Pointer to request
 * \param[out]  resp
 *              Pointer to response to fill
 * \return      APP_RES_PROTO_OK or error code
 */
app_proto_res_e Config_Handle_set_config_request(wp_SetConfigReq * req,
                                                 wp_SetConfigResp * resp);

/**
 * \brief   Return network address state
 * \return  true if network address is set
 */
bool Config_Get_has_network_address();

/**
 * \brief   Return network address in config cache
 * \return  network address, valid only if stack status indicate that network address is set
 */
net_addr_t Config_Get_network_address();

/**
 * \brief   Handle config cache when status changed
 */
void Config_On_stack_boot_status(uint8_t status);

/**
 * \brief   Fill event message header
 * \param   header_p pointer to header to fill
 */
void Config_Fill_event_header(wp_EventHeader * header_p);

/**
 * \brief   Fill reponse message header
 * \param   header_p pointer to header to fill
 * \param   req_id request id
 * \param   res response to request
 */
void Config_Fill_response_header(wp_ResponseHeader * header_p,
                                 uint64_t req_id,
                                 wp_ErrorCode res);

/**
 * \brief   Fill config stucture with stored config
 * \param   config_p pointer to config to fill
 */
void Config_Fill_config(wp_SinkReadConfig * config_p);

/**
 * \brief   Initialize the config module
 * \param[in]    gateway_id
 *               Pointer to gateway id string
 * \param[in]    sink_id
 *               Pointer to the sink id string
 * \return  0 if initialization succeed, an error code otherwise
 * \note    Connection with sink must be ready before calling this module
 */
int Config_Init(char * gateway_id,
                char * sink_id);

/**
 * \brief   Close the config module
 */
void Config_Close();

#endif /* SOURCE_CONFIG_H_ */
