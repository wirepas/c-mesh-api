/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PROTO_CONFIG_H_
#define PROTO_CONFIG_H_

#include "generic_message.pb.h"
#include "wpc.h"
#include "wpc_proto.h"


/**
 * \brief   Intialize the module in charge of data handling config
 * \return  True if successful, False otherwise
 */
bool Proto_config_init(void);

/**
 * \brief   Close the module in charge of config
 */
void Proto_config_close();

/**
 * \brief   Handle config cache when status changed
 */
void Proto_config_on_stack_boot_status(uint8_t status);

app_proto_res_e Proto_config_handle_set_config(wp_SetConfigReq *req,
                                               wp_SetConfigResp *resp);

app_proto_res_e Proto_config_handle_get_configs(wp_GetConfigsReq *req,
                                                wp_GetConfigsResp *resp);

app_proto_res_e Proto_config_get_current_event_status(bool online,
                                                      uint8_t * event_status_p,
                                                      size_t * event_status_size_p);

app_proto_res_e Proto_config_register_for_event_status(onEventStatus_cb_f onProtoEventStatus_cb);

#endif
