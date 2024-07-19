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

#include "generic_message.pb.h"
#include "data_message.pb.h"
#include "config_message.pb.h"
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
#define GATEWAY_ID_MAX_SIZE      16
#define GATEWAY_MODEL_MAX_SIZE   64
#define GATEWAY_VERSION_MAX_SIZE 32
#define SINK_ID_MAX_SIZE         16

// Max possible size of encoded message
#define MAX_PROTOBUF_SIZE WP_CONFIG_MESSAGE_PB_H_MAX_SIZE


/**
 * \brief   Macro to convert dual_mcu return code
 *          to library error code based on a LUT
 *          Dual mcu return code are not harmonized so
 *          a different LUT must be used per fonction
 */
#define convert_error_code(LUT, error)                                         \
    ({                                                                         \
        app_res_e ret = wp_ErrorCode_INTERNAL_ERROR;                           \
        if (error >= 0 && error < sizeof(LUT))                                 \
        {                                                                      \
            ret = LUT[error];                                                  \
        }                                                                      \
        ret;                                                                   \
    })

/* Error code LUT for protobuf errors from app_res_e */
static const wp_ErrorCode APP_ERROR_CODE_LUT[] = {
    wp_ErrorCode_OK                             ,   // APP_RES_OK,                           Everything is ok
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_NOT_STOPPED,            Stack is not stopped
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_ALREADY_STOPPED,        Stack is already stopped
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_ALREADY_STARTED,        Stack is already started
    wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_VALUE,                A parameter has an invalid value
    wp_ErrorCode_INVALID_ROLE                   ,   // APP_RES_ROLE_NOT_SET,                 The node role is not set
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_NODE_ADD_NOT_SET,             The node address is not set
    wp_ErrorCode_INVALID_NETWORK_ADDRESS        ,   // APP_RES_NET_ADD_NOT_SET,              The network address is not set
    wp_ErrorCode_INVALID_NETWORK_CHANNEL        ,   // APP_RES_NET_CHAN_NOT_SET,             The network channel is not set
    wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_STACK_IS_STOPPED,             Stack is stopped
    wp_ErrorCode_INVALID_ROLE                   ,   // APP_RES_NODE_NOT_A_SINK,              Node is not a sink
    wp_ErrorCode_INVALID_DEST_ADDRESS           ,   // APP_RES_UNKNOWN_DEST,                 Unknown destination address
    wp_ErrorCode_INVALID_APP_CONFIG             ,   // APP_RES_NO_CONFIG,                    No configuration received/set
wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_ALREADY_REGISTERED,           Cannot register several times
wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_NOT_REGISTERED,               Cannot unregister if not registered first
wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_ATTRIBUTE_NOT_SET,            Attribute is not set yet
    wp_ErrorCode_ACCESS_DENIED                  ,   // APP_RES_ACCESS_DENIED,                Access denied
    wp_ErrorCode_INVALID_DATA_PAYLOAD           ,   // APP_RES_DATA_ERROR,                   Error in data
wp_ErrorCode_INVALID_SINK_STATE                 ,   // APP_RES_NO_SCRATCHPAD_START,          No scratchpad start request sent
    wp_ErrorCode_INVALID_SCRATCHPAD             ,   // APP_RES_NO_VALID_SCRATCHPAD,          No valid scratchpad
wp_ErrorCode_INVALID_SINK_STATE             ,   // APP_RES_NOT_A_SINK,                   Stack is not a sink
    wp_ErrorCode_SINK_OUT_OF_MEMORY             ,   // APP_RES_OUT_OF_MEMORY,                Out of memory
    wp_ErrorCode_INVALID_DIAG_INTERVAL          ,   // APP_RES_INVALID_DIAG_INTERVAL,        Invalid diag interval
    wp_ErrorCode_INVALID_SEQUENCE_NUMBER        ,   // APP_RES_INVALID_SEQ,                  Invalid sequence number
wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_START_ADDRESS,        Start address is invalid
wp_ErrorCode_INVALID_PARAM                  ,   // APP_RES_INVALID_NUMBER_OF_BYTES,      Invalid number of bytes
    wp_ErrorCode_INVALID_SCRATCHPAD             ,   // APP_RES_INVALID_SCRATCHPAD,           Scratchpad is not valid
    wp_ErrorCode_INVALID_REBOOT_DELAY           ,   // APP_RES_INVALID_REBOOT_DELAY,         Invalid reboot delay
    wp_ErrorCode_INTERNAL_ERROR                     // APP_RES_INTERNAL_ERROR                WPC internal error
};

/* Return the size of a struct member */
#define member_size(type, member) (sizeof( ((type *)0)->member ))

static char     m_gateway_id[GATEWAY_ID_MAX_SIZE];
static char     m_gateway_model[GATEWAY_MODEL_MAX_SIZE];
static char     m_gateway_version[GATEWAY_VERSION_MAX_SIZE];
static char     m_sink_id[SINK_ID_MAX_SIZE];
static uint32_t m_network_id = 0x123456; // TODO: to be read from the sink

static uint32_t m_event_id = 0;

static onProtoDataRxEvent_cb_f m_onProtoDataRxEvent_cb = NULL;
static onProtoEventStatus_cb_f m_onProtoEventStatus_cb = NULL;

static uint8_t m_output_buffer[MAX_PROTOBUF_SIZE];

/* top messages for encoding process */
static wp_GenericMessage message_to_encode = wp_GenericMessage_init_zero;
static wp_WirepasMessage message_wirepas_to_encode = wp_WirepasMessage_init_zero;

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

    bool status;

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
        .has_network_address = true,
        .network_address = m_network_id
    };

    // Add payload
    memcpy(message_PacketReceived_p->payload.bytes, bytes, num_bytes);
    message_PacketReceived_p->payload.size = num_bytes;

    // Add the header (TODO: make a function for it)
    wp_EventHeader * header = &message_PacketReceived_p->header;

    _Static_assert(member_size(wp_EventHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_EventHeader, sink_id) >= SINK_ID_MAX_SIZE);

    strncpy(header->gw_id, m_gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE] = '\0';
    strncpy(header->sink_id, m_sink_id, SINK_ID_MAX_SIZE);
    m_gateway_id[SINK_ID_MAX_SIZE] = '\0';
    header->has_sink_id            = (strlen(m_sink_id) != 0);
    header->has_time_ms_epoch      = true;
    header->time_ms_epoch          = Platform_get_timestamp_ms_epoch();
    header->event_id               = m_event_id++;


    // Using the module static buffer
    pb_ostream_t stream
        = pb_ostream_from_buffer(m_output_buffer, sizeof(m_output_buffer));

    /* Now we are ready to encode the message! */
    status = pb_encode(&stream, wp_GenericMessage_fields, &message_to_encode);
    /* free temporary message struct */
    Platform_free(message_PacketReceived_p, sizeof(wp_PacketReceivedEvent));

    if (!status)
    {
        LOGE("Encoding PacketReceived failed: %s\n", PB_GET_ERROR(&stream));
    }
    else
    {
        LOGI("Msg size %d\n", stream.bytes_written);
        if (m_onProtoDataRxEvent_cb != NULL)
        {
            m_onProtoDataRxEvent_cb(m_output_buffer,
                                    stream.bytes_written,
                                    m_network_id,
                                    src_ep,
                                    dst_ep);
        }
    }

    return true;
}

static void onStackStatusReceived(uint8_t status)
{
    _Static_assert(member_size(wp_StatusEvent,gw_model) >= GATEWAY_MODEL_MAX_SIZE);
    _Static_assert(member_size(wp_StatusEvent,gw_version) >= GATEWAY_VERSION_MAX_SIZE);
    _Static_assert(wp_StatusEvent_size <= sizeof(m_output_buffer));

    memset(&message_wirepas_to_encode, 0, sizeof(message_wirepas_to_encode));

    LOGI("Status received : %d\n", status);

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
        .state   = ((status & 0x01) ? wp_OnOffState_ON : wp_OnOffState_OFF),
        .configs_count = 0
    };

    /* TODO : fill message_StatusEvent_p->configs[0] */
    memset(&message_StatusEvent_p->configs[0], 0, sizeof(wp_SinkReadConfig));

    message_StatusEvent_p->has_gw_model   = (strlen(m_gateway_model) != 0);
    message_StatusEvent_p->has_gw_version = (strlen(m_gateway_version) != 0);
    strncpy(message_StatusEvent_p->gw_model,
            m_gateway_model,
            GATEWAY_MODEL_MAX_SIZE);
    m_gateway_id[GATEWAY_MODEL_MAX_SIZE] = '\0';
    strncpy(message_StatusEvent_p->gw_version,
            m_gateway_version,
            GATEWAY_VERSION_MAX_SIZE);
    m_gateway_id[GATEWAY_VERSION_MAX_SIZE] = '\0';

    // Add the header (TODO: make a function for it)
    wp_EventHeader * header = &message_StatusEvent_p->header;

    _Static_assert(member_size(wp_EventHeader, gw_id) >= GATEWAY_ID_MAX_SIZE);
    _Static_assert(member_size(wp_EventHeader, sink_id) >= SINK_ID_MAX_SIZE);

    strncpy(header->gw_id, m_gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE] = '\0';
    strncpy(header->sink_id, m_sink_id, SINK_ID_MAX_SIZE);
    m_gateway_id[SINK_ID_MAX_SIZE] = '\0';
    header->has_sink_id            = (strlen(m_sink_id) != 0);
    header->has_time_ms_epoch      = true;
    header->time_ms_epoch          = Platform_get_timestamp_ms_epoch();
    header->event_id               = m_event_id++;

    // Using the module static buffer
    pb_ostream_t stream
        = pb_ostream_from_buffer(m_output_buffer, sizeof(m_output_buffer));

    /* Now we are ready to encode the message! */
    status = pb_encode(&stream, wp_GenericMessage_fields, &message_to_encode);
    /* free temporary message struct */
    Platform_free(message_StatusEvent_p, sizeof(wp_StatusEvent));

    if (!status)
    {
        LOGE("Encoding StatusEvent failed: %s\n", PB_GET_ERROR(&stream));
    }
    else
    {
        LOGI("Msg size %d\n", stream.bytes_written);
        if (m_onProtoEventStatus_cb != NULL)
        {
            m_onProtoEventStatus_cb(m_output_buffer, stream.bytes_written);
        }
    }
}

app_proto_res_e WPC_Proto_initialize(char * gateway_id,
                                     char * gateway_model,
                                     char * gateway_version,
                                     char * sink_id)
{

    strncpy(m_gateway_id, gateway_id, GATEWAY_ID_MAX_SIZE);
    m_gateway_id[GATEWAY_ID_MAX_SIZE-1]='\0';

    strncpy(m_gateway_model, gateway_model, GATEWAY_MODEL_MAX_SIZE);
    m_gateway_id[GATEWAY_MODEL_MAX_SIZE-1]='\0';

    strncpy(m_gateway_version, gateway_version, GATEWAY_VERSION_MAX_SIZE);
    m_gateway_id[GATEWAY_VERSION_MAX_SIZE-1]='\0';

    strncpy(m_sink_id, sink_id, SINK_ID_MAX_SIZE);
    m_sink_id[SINK_ID_MAX_SIZE-1]='\0';

    LOGI("WPC proto initialize with gw_id %s, model %s, ver %s and sink_id %s\n",
                m_gateway_id,
                m_gateway_model,
                m_gateway_version,
                m_sink_id);


    /* Register for all data */

    message_to_encode.wirepas = &message_wirepas_to_encode;

    WPC_register_for_data(onDataReceived);

    if (WPC_register_for_stack_status(onStackStatusReceived) != APP_RES_OK)
    {
        LOGE("Stack status already registered\n");
        return APP_RES_PROTO_ALREADY_REGISTERED;
    }

    return APP_RES_PROTO_OK;
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
        .res = convert_error_code(APP_ERROR_CODE_LUT, res),
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