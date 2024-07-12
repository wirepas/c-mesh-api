/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
 #define PRINT_BUFFERS
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


app_proto_res_e WPC_register_for_data_rx_event(onDataRxEvent_cb_f onDataRxEvent_cb)
{
    // Only support one "client" for now
    if (m_rx_event_cb != NULL)
    {
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    m_rx_event_cb = onDataRxEvent_cb;
    return APP_RES_PROTO_OK;
}
