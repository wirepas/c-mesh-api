/* Copyright 2019 Wirepas Ltd licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include "wpc.h"
#include "wp_global.pb.h"
#include "config_message.pb.h"

/* Return the size of a struct member */
#define member_size(type, member) (sizeof( ((type *)0)->member ))

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
                                 uint64_t            req_id,
                                 wp_ErrorCode        res);

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
