/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_PROTO_H__
#define WPC_PROTO_H__

#include <stdbool.h>
#include <stdint.h>


/**
 * \brief   Return code
 */
typedef enum
{
    APP_RES_PROTO_OK,                     //!< Everything is ok
    APP_RES_PROTO_WPC_NOT_INITIALIZED,    //!< Lower level part is not initialized
    APP_RES_PROTO_ALREADY_REGISTERED,
    APP_RES_PROTO_INVALID_REQUEST,        //!< Invalid proto format
} app_proto_res_e;


app_proto_res_e WPC_Proto_initialize(const char * port_name,
                                     unsigned long bitrate,
                                     char * gateway_id,
                                     char * sink_id);

/**
 * \brief       Handle a request in protobuf format
 * \param[in]   request_p
 *              Pointer to the protobuf message
 * \param[in]   request_size
 *              Size of the request
 * \param[out]  response_p
 *              Pointer to point to the generated proto answer
 *              Set to NULL in case return code is different from APP_RES_PROTO_OK
 * \param[out]  response_size
 *              Pointer to the size of the answer.
 *              ASet to 0 in case return code is different from APP_RES_PROTO_OK
 *
 * \return      Return code of the operation
 * \note        The execution time of this request can be quite long as
 *              it may involves multiple uart communication.
 */
app_proto_res_e WPC_Proto_handle_request(uint8_t * request_p,
                                         size_t request_size,
                                         uint8_t ** response_p,
                                         size_t * response_size);


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
app_proto_res_e WPC_register_for_data_rx_event(onDataRxEvent_cb_f onDataRxEvent_cb);

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
app_proto_res_e WPC_register_for_event_status(onEventStatus_cb_f onEventStatus_cb);


#endif
