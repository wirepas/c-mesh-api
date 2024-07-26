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


bool Proto_config_init(void)
{
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