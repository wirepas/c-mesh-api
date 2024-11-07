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

#define INVALID_CURRENT_SEQ     (uint32_t) (-1)

// Used to stored the current scratchpad id being used
// for uploading chunks of scratchpad
static uint32_t m_scratchpad_load_current_seq = INVALID_CURRENT_SEQ;

static bool m_restart_after_load = false;

bool Proto_otap_init(void)
{
    return true;
}

void Proto_otap_close(void)
{

}

static app_res_e handle_scratchpad_chunk(uint8_t * chunk,
                                         size_t chunk_size,
                                         uint32_t offset,
                                         size_t total_size,
                                         uint32_t seq)
{
    app_res_e res;
    LOGD("Uploading scratchpad chunk: size %u, offset %u, total_size %u, seq %u\n",
                            chunk_size,
                            offset,
                            total_size,
                            seq);

    /* Check if it is first chunk and no other load ongoing */
    if (offset == 0)
    {
        if (m_scratchpad_load_current_seq != INVALID_CURRENT_SEQ)
        {
            LOGW("Previous upload not finished: old seq %u, new seq %u\n",
                                m_scratchpad_load_current_seq,
                                seq);

            /* Only a log, keep going with it */
        }

        m_scratchpad_load_current_seq = seq;
        res = WPC_start_local_scratchpad_update(total_size, seq);
        if (res != APP_RES_OK)
        {
            m_scratchpad_load_current_seq = INVALID_CURRENT_SEQ;
            return res;
        }
    }

    /* Check chunk is from valid scratchpad */
    if (seq != m_scratchpad_load_current_seq)
    {
        LOGE("Invalid scratchpad seq %u vs %u expected\n",
                        seq,
                        m_scratchpad_load_current_seq);
        return APP_RES_INVALID_SEQ;
    }

    /* Load the chunk */
    res = WPC_upload_local_block_scratchpad(chunk_size, chunk, offset);
    if (res != APP_RES_OK)
    {
        return res;
    }

    /* Check if it is the last one */
    if ((offset + chunk_size) == total_size)
    {
        /* Yes it is */
        m_scratchpad_load_current_seq = INVALID_CURRENT_SEQ;
    }

    return APP_RES_OK;
}

app_proto_res_e Proto_otap_handle_upload_scratchpad(wp_UploadScratchpadReq *req,
                                                    wp_UploadScratchpadResp *resp)
{
    app_res_e res = APP_RES_OK;
    uint8_t status;

    /* Check parameters */
    /* Sink only supports seq on 8 bits even if gateway api supports up to 32bits value */
    if (req->seq > UINT8_MAX)
    {
        LOGE("Scratchpad sequence number too large\n");
        
        Common_Fill_response_header(&resp->header,
                                    req->header.req_id,
                                    Common_convert_error_code(APP_RES_INVALID_VALUE));

        return APP_RES_PROTO_OK;
    }

    if ((WPC_get_stack_status(&status) == APP_RES_OK)
        && (status == 0))
    {
        /* Stack must be stoped */
        /* No action required on fail as next step will anyway fail */
        WPC_set_autostart(0);
        if (WPC_stop_stack() != APP_RES_OK)
        {
            LOGE("Upload scratchpad : Stack stop failed\n");
        }
        else
        {
            LOGI("Upload scratchpad : Stack stopped\n");
        }
        m_restart_after_load = true;
    }

    if(req->has_scratchpad)
    {
        if (req->has_chunk_info)
        {
            /* This scratchpad is a chunk */
            res = handle_scratchpad_chunk(req->scratchpad.bytes,
                                          req->scratchpad.size,
                                          req->chunk_info.start_offset,
                                          req->chunk_info.scratchpad_total_size,
                                          req->seq);
        }
        else
        {
            /* Send the full file to the sink */
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
            m_scratchpad_load_current_seq = INVALID_CURRENT_SEQ;
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
        m_scratchpad_load_current_seq = INVALID_CURRENT_SEQ;
    }

    if ((m_restart_after_load)
        && (m_scratchpad_load_current_seq == INVALID_CURRENT_SEQ))
    {
        // Restart the stack, as there is no more load in progress
        app_res_e restart_res = WPC_start_stack();
        if (restart_res != APP_RES_OK)
        {
            LOGE("Cannot restart stack after scratchpad update\n");
        }
        else
        {
            WPC_set_autostart(1);
            LOGI("Upload scratchpad : Stack started\n");
        }
        m_restart_after_load = false;

        if (res == APP_RES_OK && restart_res != APP_RES_OK)
        {
            // Overide "main" res only if it was a success
            // and restart was failling. Otherwise keep the
            // main res as it is more important
            res = restart_res;
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
    app_res_e global_res = APP_RES_OK;
    uint8_t status;
    bool restart_after_process = false;

    // TODO: Add some sanity checks

    if ((WPC_get_stack_status(&status) == APP_RES_OK)
        && (status == 0))
    {
        /* Stack must be stoped */
        /* No action required on fail as next step will anyway fail */
        WPC_set_autostart(0);
        if (WPC_stop_stack() != APP_RES_OK)
        {
            LOGE("Process scratchpad : Stack stop failed\n");
        }
        else
        {
            LOGI("Process scratchpad : Stack stopped\n");
        }
        restart_after_process = true;
    }

    global_res = WPC_update_local_scratchpad();
    if (global_res != APP_RES_OK)
    {
        LOGE("WPC_update_local_scratchpad failed %d\n", global_res);
    }
    else
    {
        /* Node must be rebooted to process the scratchpad */
        global_res = WPC_stop_stack();
        if (global_res != APP_RES_OK)
        {
            LOGE("WPC_stop_stack failed after update %d", global_res);
        }
    }

    if (restart_after_process)
    {
        /* Start the stack */
        /* Config will be read back after the restart */
        res = WPC_start_stack();
        if (res != APP_RES_OK)
        {
            LOGE("Stack start failed after reboot %d\n", res);
            if (global_res == APP_RES_OK)
            {
                // Update global_res only if was success
                global_res = res;
            }
        }
        else
        {
            WPC_set_autostart(1);
        }
    }

    Common_Fill_response_header(&resp->header,
                                req->header.req_id,
                                Common_convert_error_code(global_res));

    return APP_RES_PROTO_OK;
}