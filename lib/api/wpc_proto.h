/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_PROTO_H__
#define WPC_PROTO_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Maximum size of a response. It can be used as an hint
 * to give a big enough buffer to @ref WPC_Proto_handle_request
 * note : it should be higher than this response size list, as it's a wp_GenericMessage
 * wp_GetConfigsResp_size
 * wp_GetGwInfoResp_size
 * wp_SetConfigResp_size
 * wp_SendPacketResp_size
 * wp_GetScratchpadStatusResp_size
 * wp_ProcessScratchpadResp_size
 * wp_SetScratchpadTargetAndActionResp_size
 * wp_UploadScratchpadResp_size
 */
#define WPC_PROTO_MAX_RESPONSE_SIZE 450 

/*
 * Maximum size of an Event Status. It can be used as an hint
 * to give a big enough buffer to @WPC_Proto_get_current_event_status
 * or @onStackStatusReceived
 * note : it should be higher than wp_StatusEvent_size, as it's a wp_GenericMessage
 */
#define WPC_PROTO_MAX_EVENTSTATUS_SIZE 550

/*
 * Maximum size offset for data reception. Added to payload size, it can be used as an hint
 * to give a big enough buffer to @onDataReceived
 * note : see wp_SendPacketReq_size compared to payload size defined in wp_PacketReceivedEvent_payload_t
 * assuming that is will be a wp_GenericMessage
 */
#define WPC_PROTO_OFFSET_DATA_SIZE 100

/**
 * \brief   Return code
 */
typedef enum
{
    APP_RES_PROTO_OK,                     //!< Everything is ok
    APP_RES_PROTO_WPC_NOT_INITIALIZED,    //!< Lower level part is not initialized
    APP_RES_PROTO_ALREADY_REGISTERED,
    APP_RES_PROTO_INVALID_REQUEST,        //!< Invalid proto format
    APP_RES_PROTO_CANNOT_GENERATE_RESPONSE,
    APP_RES_PROTO_RESPONSE_BUFFER_TOO_SMALL,
    APP_RES_PROTO_NOT_ENOUGH_MEMORY,
    APP_RES_PROTO_NOT_IMPLEMENTED,
} app_proto_res_e;


/**
 * \brief        Init protobuf interface
 * \param[in]    port_name
 *               Serial port to use to connect to sink
 * \param[in]    bitrate
 *               Serial connection speed
 * \param[in]    gateway_id
 *               Pointer to gateway id string
 * \param[in]    gateway_model
 *               Pointer to gateway model string, "" if not available
 * \param[in]    gateway_version
 *               Pointer to gateway version string, "" if not available
 * \param[in]    sink_id
 *               Pointer to the sink id string
 * \return       Return code of the operation
 */
app_proto_res_e WPC_Proto_initialize(const char * port_name,
                                     unsigned long bitrate,
                                     char * gateway_id,
                                     char * gateway_model,
                                     char * gateway_version,
                                     char * sink_id);

/**
 * \brief        close protobuf interface
 */
void WPC_Proto_close();

/**
 * \brief        Handle a request in protobuf format
 * \param[in]    request_p
 *               Pointer to the protobuf message
 * \param[in]    request_size
 *               Size of the request
 * \param[out]   response_p
 *               Pointer to buffer to store the response
 *               Not updated in case return code is different from APP_RES_PROTO_OK
 * \param[inout] response_size
 *               Pointer to the size of the answer
 *               Caller [in] must set it to the max size of response buffer
 *               Callee [out] will update it to the size of the generated answer
 *               Set to 0 in case return code is different from APP_RES_PROTO_OK
 *
 * \return       Return code of the operation
 * \note         The execution time of this request can be quite long as
 *               it may involves multiple uart communication.
 *               Buffer to store the response should bigger or equal to @WPC_PROTO_MAX_RESPONSE_SIZE
 */
app_proto_res_e WPC_Proto_handle_request(const uint8_t * request_p,
                                         size_t request_size,
                                         uint8_t * response_p,
                                         size_t * response_size_p);


/**
 * \brief   Callback definition for event status
 * \param   ...
 */
typedef void (*onDataRxEvent_cb_f)(uint8_t * event_p,
                                   size_t event_size,
                                   uint32_t network_id,
                                   uint16_t src_ep,
                                   uint16_t dst_ep);

/**
 * \brief   Register for receiving event status
 * \param   onEventStatus_cb
 */
app_proto_res_e WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb_f onDataRxEvent_cb);

/**
 * \brief   Callback definition for event status
 * \param   ...
 */
typedef void (*onEventStatus_cb_f)(uint8_t * event_p,
                                   size_t event_size);

/**
 * \brief   Register for receiving event status
 * \param   onEventStatus_cb
 */
app_proto_res_e WPC_Proto_register_for_event_status(onEventStatus_cb_f onEventStatus_cb);

/**
 * \brief        Get current event Status. It is mainly required at boot time of gateway
 *               to publish Gateway status (and no event status are generated yet)
 * \param[in]    online
 *               If true, get the "ONLINE" current status. If false, get the "OFFLINE" status
 *               to be published as last will topic
 * \param[out]   event_status_p
 *               Pointer to buffer to store the event status
 *               Not updated in case return code is different from APP_RES_PROTO_OK
 * \param[inout] event_status_size_p
 *               Pointer to the size of the event status
 *               Caller [in] must set it to the max size of buffer
 *               Callee [out] will update it to the size of the generated buffer
 *               Set to 0 in case return code is different from APP_RES_PROTO_OK
 * \note         Buffer size should higher or equal to @WPC_PROTO_MAX_EVENTSTATUS_SIZE
 */
app_proto_res_e WPC_Proto_get_current_event_status(bool online,
                                                   uint8_t * event_status_p,
                                                   size_t * event_status_size_p);

#endif
