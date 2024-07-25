/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include "proto_data.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "platform.h"
#include "common.h"

#define LOG_MODULE_NAME "data_proto"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"


static uint32_t m_network_id = 0x123456; // TODO: to be read from the sink

static uint32_t m_event_id = 0;

static onDataRxEvent_cb_f m_rx_event_cb = NULL;

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
    wp_PacketReceivedEvent * message_PacketReceived_p;
    uint8_t * encoded_message_p;
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
    message_PacketReceived_p = Platform_malloc(sizeof(wp_PacketReceivedEvent));
    if (message_PacketReceived_p == NULL)
    {
        LOGE("Not enough memory to encode");
        return false;
    }

    // Allocate needed buffer for encoded message
    encoded_message_p = Platform_malloc(WPC_PROTO_MAX_RESPONSE_SIZE);
    if (encoded_message_p == NULL)
    {
        LOGE("Not enough memory for output buffer");
        Platform_free(message_PacketReceived_p, sizeof(wp_PacketReceivedEvent));
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
    strcpy(message_PacketReceived_p->header.gw_id, Common_get_gateway_id());
    strcpy(message_PacketReceived_p->header.sink_id, Common_get_sink_id());
    message_PacketReceived_p->header.has_sink_id = true;
    message_PacketReceived_p->header.has_time_ms_epoch = true;
    message_PacketReceived_p->header.time_ms_epoch = Platform_get_timestamp_ms_epoch();
    message_PacketReceived_p->header.event_id = m_event_id++;


    // Using the module static buffer
    pb_ostream_t stream = pb_ostream_from_buffer(encoded_message_p, WPC_PROTO_MAX_RESPONSE_SIZE);

    /* Now we are ready to encode the message! */
	status = pb_encode(&stream, wp_GenericMessage_fields, &message);

    /* Release buffer as we don't need it anymore */
    Platform_free(message_PacketReceived_p, sizeof(wp_PacketReceivedEvent));

	if (!status) {
		LOGE("Encoding failed: %s\n", PB_GET_ERROR(&stream));
	}
	else
	{
		LOGI("Msg size %d\n", stream.bytes_written);
        if (m_rx_event_cb != NULL)
        {
            m_rx_event_cb(encoded_message_p, stream.bytes_written,
                          m_network_id,
                          src_ep,
                          dst_ep);
        }
    }

    Platform_free(encoded_message_p, WPC_PROTO_MAX_RESPONSE_SIZE);

    return true;
}

bool Proto_data_init(void)
{
    /* Register for all data */
    return WPC_register_for_data(onDataReceived) == APP_RES_OK;
}

app_proto_res_e Proto_data_handle_send_data(wp_SendPacketReq *req,
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
        .res = Common_convert_error_code(res),
        .has_time_ms_epoch = true,
        .time_ms_epoch = Platform_get_timestamp_ms_epoch(),
    };

    strcpy(resp->header.gw_id, Common_get_gateway_id());
    strcpy(resp->header.sink_id, Common_get_sink_id());

    return APP_RES_PROTO_OK;
}

app_proto_res_e Proto_data_register_for_data(onDataRxEvent_cb_f onDataRxEvent_cb)
{
    // Only support one "client" for now
    if (m_rx_event_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_rx_event_cb = onDataRxEvent_cb;
    return APP_RES_PROTO_OK;

}