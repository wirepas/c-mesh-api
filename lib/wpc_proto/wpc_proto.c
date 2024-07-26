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
#include <assert.h>


#include "wpc.h"
#include "wpc_proto.h"
#include "platform.h"
#include "config.h"

#include <pb_encode.h>
#include <pb_decode.h>

// Wirepas Gateway's protobuff message definition version
// see lib/wpc_proto/deps/backend-apis/gateway_to_backend/README.md
#define GW_PROTO_MESSAGE_VERSION 2

// API version implemented in the gateway
// to fill implemented_api_version in GatewayInfo
// see lib/wpc_proto/deps/backend-apis/gateway_to_backend/protocol_buffers_files/config_message.proto
#define GW_PROTO_API_VERSION 1

/* Set the max of strings used (including \0), matching values defined in proto
 * files */
#define GATEWAY_MODEL_MAX_SIZE   64
#define GATEWAY_VERSION_MAX_SIZE 32

// Max possible size of encoded message
#define MAX_PROTOBUF_SIZE WP_CONFIG_MESSAGE_PB_H_MAX_SIZE

static char m_gateway_model[GATEWAY_MODEL_MAX_SIZE];
static char m_gateway_version[GATEWAY_VERSION_MAX_SIZE];

static onProtoDataRxEvent_cb_f m_onProtoDataRxEvent_cb = NULL;
static onProtoEventStatus_cb_f m_onProtoEventStatus_cb = NULL;

static uint8_t m_output_buffer[MAX_PROTOBUF_SIZE];

/* top messages for encoding process */
static wp_GenericMessage message_to_encode = wp_GenericMessage_init_zero;
static wp_WirepasMessage message_wirepas_to_encode = wp_WirepasMessage_init_zero;

// encode message_to_encode in m_output_buffer in protobuf format
// Message have to be filled before calling
// Return the size of the encoded part, or 0 in case of failure
size_t encode_wirepas_message(void)
{
    // Using the module static buffer
    pb_ostream_t stream
        = pb_ostream_from_buffer(m_output_buffer, sizeof(m_output_buffer));

    // Now we are ready to encode the message!
    if (!pb_encode(&stream, wp_GenericMessage_fields, &message_to_encode))
    {
        LOGE("Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 0;
    }

    return stream.bytes_written;
}

static bool onDataReceived(const uint8_t * bytes,
                           size_t num_bytes,
                           app_addr_t src_addr,
                           app_addr_t dst_addr,
                           app_qos_e qos,
                           uint8_t src_ep,
                           uint8_t dst_ep,
                           uint32_t travel_time,
                           uint8_t hop_count,
                           unsigned long long timestamp_ms)
{
    _Static_assert(wp_PacketReceivedEvent_size <= sizeof(m_output_buffer));

    memset(&message_wirepas_to_encode, 0, sizeof(message_wirepas_to_encode));

    LOGI("%llu -> Data received on EP %d of len %d from 0x%x to 0x%x\n",
         timestamp_ms,
         dst_ep,
         num_bytes,
         src_addr,
         dst_addr);

    // Allocate the needed space for only the submessage we want to send
    wp_PacketReceivedEvent * message_PacketReceived_p
        = Platform_malloc(sizeof(wp_PacketReceivedEvent));
    if (message_PacketReceived_p == NULL)
    {
        LOGE("Not enough memory to encode PacketReceived");
        return false;
    }

    message_wirepas_to_encode.packet_received_event = message_PacketReceived_p;

    *message_PacketReceived_p = (wp_PacketReceivedEvent) {
        .source_address = src_addr,
        .destination_address = dst_addr,
        .source_endpoint = src_ep,
        .destination_endpoint = dst_ep,
        .travel_time_ms = travel_time,
        .rx_time_ms_epoch = timestamp_ms,
        .qos = qos,
        .has_payload = true,
        .has_payload_size = false,
        .has_hop_count = true,
        .hop_count = hop_count,
        .has_network_address = Config_Get_has_network_address(),
        .network_address = Config_Get_network_address(),
    };

    // Add payload
    memcpy(message_PacketReceived_p->payload.bytes, bytes, num_bytes);
    message_PacketReceived_p->payload.size = num_bytes;

    Config_Fill_event_header(&message_PacketReceived_p->header);

    size_t protobuf_size = encode_wirepas_message();

    Platform_free(message_PacketReceived_p, sizeof(wp_PacketReceivedEvent));

    if (protobuf_size != 0)
    {
        LOGI("Msg size %d\n", protobuf_size);
        if (m_onProtoDataRxEvent_cb != NULL)
        {
            m_onProtoDataRxEvent_cb(m_output_buffer,
                                    protobuf_size,
                                    Config_Get_network_address(),
                                    src_ep,
                                    dst_ep);
        }
        return true;
    }

    return false;
}

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
        LOGE("Cannot establish communication with sink with baudrate %d bps\n", baudrate);
        WPC_close();
        return -1;
    }

    LOGI("Node is running mesh API version %d (uart baudrate is %d bps)\n", mesh_version, baudrate);
    return 0;
}

static void onStackStatusReceived(uint8_t status)
{
    _Static_assert(wp_StatusEvent_size <= sizeof(m_output_buffer));
    _Static_assert(member_size(wp_StatusEvent, gw_model) >= GATEWAY_MODEL_MAX_SIZE);
    _Static_assert(member_size(wp_StatusEvent, gw_version) >= GATEWAY_VERSION_MAX_SIZE);
 
    memset(&message_wirepas_to_encode, 0, sizeof(message_wirepas_to_encode));

    LOGI("Status received : %d\n", status);

    Config_On_stack_boot_status(status);

    // Allocate the needed space for only the submessage we want to send
    wp_StatusEvent * message_StatusEvent_p
        = Platform_malloc(sizeof(wp_StatusEvent));
    if (message_StatusEvent_p == NULL)
    {
        LOGE("Not enough memory to encode StatusEvent");
        return;
    }

    message_wirepas_to_encode.status_event = message_StatusEvent_p;

    *message_StatusEvent_p = (wp_StatusEvent){
        .version = GW_PROTO_MESSAGE_VERSION,
        .state   = ((status & APP_STACK_STOPPED) ? wp_OnOffState_OFF : wp_OnOffState_ON),
        .configs_count = 0,
    };

    message_StatusEvent_p->has_gw_model = (strlen(m_gateway_model) != 0);
    strncpy(message_StatusEvent_p->gw_model,
            m_gateway_model,
            member_size(wp_StatusEvent, has_gw_model));
    message_StatusEvent_p->gw_model[member_size(wp_StatusEvent, has_gw_model) - 1] = '\0';

    message_StatusEvent_p->has_gw_version = (strlen(m_gateway_version) != 0);
    strncpy(message_StatusEvent_p->gw_version,
            m_gateway_version,
            member_size(wp_StatusEvent, gw_version));
    message_StatusEvent_p->gw_version[member_size(wp_StatusEvent, gw_version) - 1] = '\0';

    Config_Fill_config(&message_StatusEvent_p->configs[0]);
    
    Config_Fill_event_header(&message_StatusEvent_p->header);

    size_t protobuf_size = encode_wirepas_message();

    Platform_free(message_StatusEvent_p, sizeof(wp_StatusEvent));

    if (protobuf_size != 0)
    {
        LOGI("Msg size %d\n", protobuf_size);
        if (m_onProtoEventStatus_cb != NULL)
        {
            m_onProtoEventStatus_cb(m_output_buffer, protobuf_size);
        }
    }
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

    strncpy(m_gateway_model, gateway_model, GATEWAY_MODEL_MAX_SIZE);
    m_gateway_model[GATEWAY_MODEL_MAX_SIZE - 1] = '\0';

    strncpy(m_gateway_version, gateway_version, GATEWAY_VERSION_MAX_SIZE);
    m_gateway_version[GATEWAY_VERSION_MAX_SIZE - 1] = '\0';

    LOGI("WPC proto initialize with model %s, ver %s\n",
                m_gateway_model,
                m_gateway_version);

    Config_Init(gateway_id, sink_id);

    /* Register for all data */

    message_to_encode.wirepas = &message_wirepas_to_encode;

    if (WPC_register_for_stack_status(onStackStatusReceived) != APP_RES_OK)
    {
        LOGE("Stack status already registered\n");
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    WPC_register_for_data(onDataReceived);

    return APP_RES_PROTO_OK;
}

void WPC_Proto_close(void)
{
    Config_Close();
    WPC_close();
}

app_proto_res_e WPC_Proto_register_for_data_rx_event(onProtoDataRxEvent_cb_f onProtoDataRxEvent_cb)
{
    // Only support one "client" for now
    if (m_onProtoDataRxEvent_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_onProtoDataRxEvent_cb = onProtoDataRxEvent_cb;
    return APP_RES_PROTO_OK;
}

app_proto_res_e WPC_Proto_register_for_event_status(onProtoEventStatus_cb_f onProtoEventStatus_cb)
{
    // Only support one "client" for now
    if (m_onProtoEventStatus_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_onProtoEventStatus_cb = onProtoEventStatus_cb;
    return APP_RES_PROTO_OK;
}

/**
 * \brief   Handle Get gateway info
 * \param   req
 *          Pointer to the request received
 * \param   resp
 *          Pointer to the reponse to send back
 * \return  APP_RES_PROTO_OK if answer is ready to send
 */
app_proto_res_e handle_get_gateway_info_request(wp_GetGwInfoReq * req,
                                                wp_GetGwInfoResp * resp)
{
    app_res_e res = APP_RES_OK;

    // TODO: Add some sanity checks

    Config_Fill_response_header(&resp->header,
                                req->header.req_id,
                                convert_error_code(APP_ERROR_CODE_LUT, res));

    resp->info.current_time_s_epoch = Platform_get_timestamp_ms_epoch();

    resp->info.has_gw_model = (strlen(m_gateway_model) != 0);
    strncpy(resp->info.gw_model,
            m_gateway_model,
            member_size(wp_StatusEvent, gw_model));
    resp->info.gw_model[member_size(wp_StatusEvent, gw_model) - 1] = '\0';

    resp->info.has_gw_version = (strlen(m_gateway_version) != 0);
    strncpy(resp->info.gw_version,
            m_gateway_version,
            member_size(wp_StatusEvent, gw_version));
    resp->info.gw_version[member_size(wp_StatusEvent, gw_version) - 1] = '\0';

    resp->info.has_implemented_api_version = true;
    resp->info.implemented_api_version = GW_PROTO_API_VERSION;

    return APP_RES_PROTO_OK;
}

static app_proto_res_e handle_send_data_request(wp_SendPacketReq *req,
                                                wp_SendPacketResp *resp)
{
    app_res_e res;

    // TODO: Add some sanity checks
    res = WPC_send_data(req->payload.bytes,
                        req->payload.size,
                        0,
                        req->destination_address,
                        req->qos,
                        (uint8_t) req->source_endpoint,
                        (uint8_t) req->destination_endpoint,
                        NULL,
                        0); // No initial delay supported

    LOGI("WPC_send_data res=%d\n", res);

    Config_Fill_response_header(&resp->header,
                                req->header.req_id,
                                convert_error_code(APP_ERROR_CODE_LUT, res));

    return APP_RES_PROTO_OK;
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
        resp_size = sizeof(wp_GetConfigsResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode GetConfigResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->get_configs_resp = (wp_GetConfigsResp *) resp_msg_p;

        LOGI("Get config request\n");

        res = Config_Handle_get_config_request(wp_message_req_p->get_configs_req,
                                               (wp_GetConfigsResp *) resp_msg_p);
    }
    else if (wp_message_req_p->set_config_req)
    {
        resp_size = sizeof(wp_SetConfigResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode SetConfigResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->set_config_resp = (wp_SetConfigResp *) resp_msg_p;

        LOGI("Set config request\n");

        res = Config_Handle_set_config_request(wp_message_req_p->set_config_req,
                                               (wp_SetConfigResp *) resp_msg_p);
    }
    else if (wp_message_req_p->send_packet_req)
    {
        resp_size  = sizeof(wp_SendPacketResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode SendPacketResp");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->send_packet_resp
            = (wp_SendPacketResp *) resp_msg_p;

        LOGI("Send packet request\n");

        res = handle_send_data_request(wp_message_req_p->send_packet_req,
                                       (wp_SendPacketResp *) resp_msg_p);
    }
    else if (wp_message_req_p->get_scratchpad_status_req)
    {
        LOGI("Get scratchpad status request\n");
    }
    else if (wp_message_req_p->upload_scratchpad_req)
    {
        LOGI("Upload scratchpad request\n");
    }
    else if (wp_message_req_p->process_scratchpad_req)
    {
        LOGI("Process scratchpad request\n");
    }
    else if (wp_message_req_p->get_gateway_info_req)
    {
        resp_size = sizeof(wp_GetGwInfoResp);
        resp_msg_p = Platform_malloc(resp_size);
        if (resp_msg_p == NULL)
        {
            LOGE("Not enough memory to encode GetGatewayInfo\n");
            return APP_RES_PROTO_NOT_ENOUGH_MEMORY;
        }
        message_resp.wirepas->get_gateway_info_resp = (wp_GetGwInfoResp *) resp_msg_p;

        LOGI("Get gateway info request\n");

        res = handle_get_gateway_info_request(wp_message_req_p->get_gateway_info_req,
                                                     (wp_GetGwInfoResp *) resp_msg_p);        

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