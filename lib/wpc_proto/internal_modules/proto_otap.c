/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wpc.h"
#include "proto_otap.h"
#include "proto_config.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "platform.h"
#include "common.h"

#define LOG_MODULE_NAME "otap_proto"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

bool Proto_otap_init(void)
{
    return true;
}

void Proto_otap_close(void)
{

}

app_proto_res_e Proto_otap_handle_upload_scratchpad(wp_UploadScratchpadReq *req,
                                                    wp_UploadScratchpadResp *resp)
{
    app_res_e res = APP_RES_OK;

    // TODO: Add some sanity checks

    if(req->has_scratchpad)
    {

        /* Send the file to the sink */
        res = WPC_upload_local_scratchpad(req->scratchpad.size,
                                          req->scratchpad.bytes,
                                          req->seq);
        if (res == APP_RES_OK)
        {
            LOGI("Scratchpad uploaded : with seq %d of size %d\n", req->seq, req->scratchpad.size);
        }
        else
        {
            LOGE("Upload scratchpad failed %d: with seq %d of size %d\n", res, req->seq, req->scratchpad.size);
        }
    }
    else
    {
        // No scratchpad provided, clear existing one
        res = WPC_clear_local_scratchpad();
        if (res == APP_RES_OK)
        {
            LOGI("Scratchpad cleared\n");
        }
        else
        {
            LOGE("Clear scratchpad failed %d\n", res);
        }
    }

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(res));

    return APP_RES_PROTO_OK;
}


app_proto_res_e Proto_otap_handle_process_scratchpad(wp_ProcessScratchpadReq *req,
                                                     wp_ProcessScratchpadResp *resp)
{
    app_res_e res = APP_RES_OK;

    // TODO: Add some sanity checks

    res = WPC_update_local_scratchpad();
    if (res != APP_RES_OK)
    {
        LOGE("WPC_update_local_scratchpad failed %d\n", res);
    }
    else
    {
        /* Node must be rebooted to process the scratchpad */
        res = WPC_stop_stack();
        if (res != APP_RES_OK)
        {
            LOGE("WPC_stop_stack failed %d", res);
        }
        /* Config will be read back after the restart */
    }

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(res));

    return APP_RES_PROTO_OK;
}