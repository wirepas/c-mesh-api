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
#include "platform.h"

#include "generic_message.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>

#define GATEWAY_ID_MAX_SIZE 16
#define SINK_ID_MAX_SIZE 16

#define MAX_PROTOBUF_SIZE 1024

static char m_gateway_id[GATEWAY_ID_MAX_SIZE+1];
static char m_sink_id[SINK_ID_MAX_SIZE+1];
static uint32_t m_network_id = 0x123456; // TODO: to be read from the sink

static uint32_t m_event_id = 0;

static onDataRxEvent_cb_f m_rx_event_cb = NULL;

static uint8_t m_output_buffer[MAX_PROTOBUF_SIZE];

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
    bool status;
    wp_GenericMessage message = wp_GenericMessage_init_zero;
    wp_WirepasMessage message_wirepas = wp_WirepasMessage_init_zero;
    message.wirepas = &message_wirepas;

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
        LOGE("Not enough memory to encode");
        return false;
    }

    message_wirepas.packet_received_event = message_PacketReceived_p;

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
        .has_network_address = true,
        .network_address = m_network_id
    };

    // Add payload
    memcpy(message_PacketReceived_p->payload.bytes, bytes, num_bytes);
    message_PacketReceived_p->payload.size = num_bytes;

    // Add the header (TODO: make a function for it)
    strcpy(message_PacketReceived_p->header.gw_id, m_gateway_id);
    strcpy(message_PacketReceived_p->header.sink_id, m_sink_id);
    message_PacketReceived_p->header.has_sink_id = true;
    message_PacketReceived_p->header.has_time_ms_epoch = true;
    message_PacketReceived_p->header.time_ms_epoch = Platform_get_timestamp_ms_epoch();
    message_PacketReceived_p->header.event_id = m_event_id++;


    // Using the module static buffer
    pb_ostream_t stream = pb_ostream_from_buffer(m_output_buffer, sizeof(m_output_buffer));

    /* Now we are ready to encode the message! */
	status = pb_encode(&stream, wp_GenericMessage_fields, &message);

	if (!status) {
		LOGE("Encoding failed: %s\n", PB_GET_ERROR(&stream));
	}
	else
	{
		LOGI("Msg size %d\n", stream.bytes_written);
        if (m_rx_event_cb != NULL)
        {
            m_rx_event_cb(m_output_buffer, stream.bytes_written,
                          m_network_id,
                          src_ep,
                          dst_ep);
        }
    }
    Platform_free(message_PacketReceived_p, sizeof(wp_PacketReceivedEvent));

    return true;
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
                                     char * sink_id)
{
    if (open_and_check_connection(bitrate, port_name) != 0)
    {
        return APP_RES_PROTO_WPC_NOT_INITIALIZED;
    }

    strncpy(m_gateway_id, gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE]='\0';

    strncpy(m_sink_id, sink_id, SINK_ID_MAX_SIZE);
    m_sink_id[SINK_ID_MAX_SIZE]='\0';

    LOGI("WPC proto initialize with gw_id = %s and sink_id = %s\n",
                m_gateway_id,
                m_sink_id);

    /* Register for all data */
    WPC_register_for_data(onDataReceived);


    return APP_RES_PROTO_OK;
}


app_proto_res_e WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb_f onDataRxEvent_cb)
{
    // Only support one "client" for now
    if (m_rx_event_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_rx_event_cb = onDataRxEvent_cb;
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

    // Fill the response
    resp->header = (wp_ResponseHeader) {
        .req_id = req->header.req_id,
        .has_sink_id = true,
        .res = res, //TODO make a conversion!!!
        .has_time_ms_epoch = true,
        .time_ms_epoch = Platform_get_timestamp_ms_epoch(),
    };

    strcpy(resp->header.gw_id, m_gateway_id);
    strcpy(resp->header.sink_id, m_sink_id);

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
    }
    else if (wp_message_req_p->set_config_req)
    {
        LOGI("Set config request\n");
    }
    else if (wp_message_req_p->send_packet_req)
    {
        wp_SendPacketResp sendResp;
        LOGI("Send packet request\n");
        message_resp.wirepas->send_packet_resp = &sendResp;
        res = handle_send_data_request(wp_message_req_p->send_packet_req,
                                &sendResp);
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
        LOGI("Get gateway info request\n");
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
            return APP_RES_CANNOT_GENERATE_RESPONSE;
        }
        else
        {
            LOGI("Response generated %d\n", stream_out.bytes_written);
            *response_size_p = stream_out.bytes_written;
            return APP_RES_PROTO_OK;
        }
    }

    /** Set correctly the response_p and response_size */
    return res;

}