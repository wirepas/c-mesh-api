/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_PROTO_H__
#define WPC_PROTO_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * Maximum size of a response. It can be used as an hint
 * to give a big enough buffer to @ref WPC_Proto_handle_request
 */
#define WPC_PROTO_MAX_RESPONSE_SIZE 512

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
} app_proto_res_e;


app_proto_res_e WPC_Proto_initialize(const char * port_name,
                                     unsigned long bitrate,
                                     char * gateway_id,
                                     char * sink_id);

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


#endif
