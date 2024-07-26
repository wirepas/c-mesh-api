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
 * \brief   Intialize the module in charge of data handling congig
 * \return  True if successful, False otherwise
 */
bool Proto_config_init(void);

app_proto_res_e Proto_config_handle_set_config(wp_SetConfigReq *req,
                                               wp_SetConfigResp *resp);

app_proto_res_e Proto_config_handle_get_configs(wp_GetConfigsReq *req,
                                                wp_GetConfigsResp *resp);

#endif