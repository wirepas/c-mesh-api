/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
//#define PRINT_BUFFERS
#define LOG_MODULE_NAME "wpc_proto"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include <string.h>


#include "wpc.h"
#include "wpc_proto.h"
#include "proto_data.h"
#include "proto_config.h"
#include "proto_otap.h"
#include "common.h"
#include "platform.h"

#include "generic_message.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>

// Max possible size of encoded message
#define MAX_PROTOBUF_SIZE WP_CONFIG_MESSAGE_PB_H_MAX_SIZE


static int open_and_check_connection(unsigned long baudrate, const char * port_name)
{
    uint16_t mesh_version;
    if (WPC_initialize(port_name, baudrate) != APP_RES_OK)
    {
        LOGE("Cannot open serial sink connection (%s)\n", port_name);
        return -1;
    }

    /* Check the connectivity with sink by reading mesh version */
    if (WPC_get_mesh_API_version(&mesh_version) != APP_RES_OK)
    {
        LOGD("Cannot establish communication with sink with baudrate %d bps\n", baudrate);
        WPC_close();
        return -1;
    }

    LOGI("Node is running mesh API version %d (uart baudrate is %d bps)\n", mesh_version, baudrate);
    return 0;
}

app_proto_res_e WPC_Proto_initialize(const char * port_name,
                                     unsigned long bitrate,
                                     char * gateway_id,
                                     char * gateway_model,
                                     char * gateway_version,
                                     char * sink_id)
{
    if (open_and_check_connection(bitrate, port_name) != 0)
    {
        return APP_RES_PROTO_WPC_NOT_INITIALIZED;
    }

    Common_init(gateway_id, gateway_model, gateway_version, sink_id);
    Proto_data_init();
    Proto_config_init();
    Proto_otap_init();

    LOGI("WPC proto initialized with gw_id = %s, gw_model = %s, gw_version = %s and sink_id = %s\n",
                gateway_id,
                gateway_model,
                gateway_version,
                sink_id);

    return APP_RES_PROTO_OK;
}

void WPC_Proto_close()
{
    WPC_unregister_from_stack_status();
    Proto_otap_close();
    Proto_config_close();
    Proto_data_close();
    WPC_close();
}

app_proto_res_e WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb_f onDataRxEvent_cb)
{
    return Proto_data_register_for_data(onDataRxEvent_cb);
}

app_proto_res_e WPC_Proto_register_for_event_status(onEventStatus_cb_f onProtoEventStatus_cb)
{
    return Proto_config_register_for_event_status(onProtoEventStatus_cb);
}

app_proto_res_e WPC_Proto_handle_request(const uint8_t * request_p,
                                         size_t request_size,
                                         uint8_t * response_p,
                                         size_t * response_size_p)
{
    bool status;
    app_proto_res_e res = APP_RES_PROTO_INVALID_REQUEST;
    /* All messages are generic messages */
    wp_GenericMessage message_req = wp_GenericMessage_init_zero;
    wp_WirepasMessage * wp_message_req_p = NULL;
    pb_istream_t stream_in = pb_istream_from_buffer(request_p, request_size);
    pb_ostream_t stream_out;
    void * resp_msg_p = NULL;
    size_t resp_size  = 0;


    // Prepare response
    wp_GenericMessage message_resp = wp_GenericMessage_init_zero;
    wp_WirepasMessage message_wirepas_resp = wp_WirepasMessage_init_zero;
    message_resp.wirepas = &message_wirepas_resp;

    status = pb_decode(&stream_in, wp_GenericMessage_fields, &message_req);

    if (!status)
    {
        LOGE("Decoding failed: %s\n", PB_GET_ERROR(&stream_in));
        return APP_RES_PROTO_INVALID_REQUEST;
    }

    wp_message_req_p = message_req.wirepas;
    /* Do some sanity check on request header */
    if (!wp_message_req_p)
    {
        LOGW("Not a wirepas message\n");
        return APP_RES_PROTO_INVALID_REQUEST;
    }

    /* Call the right request handler */
    if (wp_message_req_p->get_configs_req)
    {
        LOGI("Get config request\n");
        resp_size  = sizeof(wp_GetConfigsResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode GetConfigsResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->get_configs_resp
            = (wp_GetConfigsResp *) resp_msg_p;

        res = Proto_config_handle_get_configs(wp_message_req_p->get_configs_req,
                                             (wp_GetConfigsResp *) resp_msg_p);
    }
    else if (wp_message_req_p->set_config_req)
    {
        LOGI("Set config request\n");
        resp_size  = sizeof(wp_SetConfigResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode SetConfigResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->set_config_resp
            = (wp_SetConfigResp *) resp_msg_p;

        res = Proto_config_handle_set_config(wp_message_req_p->set_config_req,
                                            (wp_SetConfigResp *) resp_msg_p);
    }
    else if (wp_message_req_p->send_packet_req)
    {
        LOGI("Send packet request\n");
        resp_size  = sizeof(wp_SendPacketResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode SendPacketResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->send_packet_resp
            = (wp_SendPacketResp *) resp_msg_p;

        res = Proto_data_handle_send_data(wp_message_req_p->send_packet_req,
                                          (wp_SendPacketResp *) resp_msg_p);
    }
    else if (wp_message_req_p->get_scratchpad_status_req)
    {
        LOGI("Get scratchpad status request\n");
        resp_size  = sizeof(wp_GetScratchpadStatusResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode GetScratchpadStatusResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->get_scratchpad_status_resp
            = (wp_GetScratchpadStatusResp *) resp_msg_p;

        res = Proto_otap_handle_get_scratchpad_status(wp_message_req_p->get_scratchpad_status_req,
                                                      (wp_GetScratchpadStatusResp *) resp_msg_p);
    }
    else if (wp_message_req_p->upload_scratchpad_req)
    {
        LOGI("Upload scratchpad request\n");
        resp_size  = sizeof(wp_UploadScratchpadResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode UploadScratchpadResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->upload_scratchpad_resp
            = (wp_UploadScratchpadResp *) resp_msg_p;

        res = Proto_otap_handle_upload_scratchpad(wp_message_req_p->upload_scratchpad_req,
                                                  (wp_UploadScratchpadResp *) resp_msg_p);
    }
    else if (wp_message_req_p->process_scratchpad_req)
    {
        LOGI("Process scratchpad request\n");
    }
    else if (wp_message_req_p->get_gateway_info_req)
    {
        LOGI("Get gateway info request\n");
        resp_size = sizeof(wp_GetGwInfoResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode GetGatewayInfo\n");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->get_gateway_info_resp = (wp_GetGwInfoResp *) resp_msg_p;

        res = Proto_config_handle_get_gateway_info_request(
            wp_message_req_p->get_gateway_info_req, (wp_GetGwInfoResp *) resp_msg_p);
    }
    else
    {
        LOGE("Not a supported request\n");
    }

    if (res == APP_RES_PROTO_OK)
    {
        //Serialize the answer
        stream_out = pb_ostream_from_buffer(response_p, *response_size_p);

        /* Now we are ready to encode the message! */
        status = pb_encode(&stream_out, wp_GenericMessage_fields, &message_resp);

        if (!status) {
            LOGE("Encoding failed: %s\n", PB_GET_ERROR(&stream_out));
            return APP_RES_PROTO_CANNOT_GENERATE_RESPONSE;
        }
        else
        {
            LOGI("Response generated %d\n", stream_out.bytes_written);
            *response_size_p = stream_out.bytes_written;
            return APP_RES_PROTO_OK;
        }
    }

    Platform_free(resp_msg_p, resp_size);

    /** Set correctly the response_p and response_size */
    return res;

}

app_proto_res_e WPC_Proto_get_current_event_status(bool online,
                                                   uint8_t * event_status_p,
                                                   size_t * event_status_size_p)
{
    return Proto_config_get_current_event_status(online,
                                                 event_status_p,
                                                 event_status_size_p);
}
