/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PROTO_OTAP_H_
#define PROTO_OTAP_H_

#include "generic_message.pb.h"
#include "wpc.h"
#include "wpc_proto.h"

/**
 * \brief   Intialize the module in charge of otap
 * \return  True if successful, False otherwise
 */
bool Proto_otap_init(void);

/**
 * \brief   Close the module in charge of data
 */
void Proto_otap_close(void);

wp_ScratchpadType Proto_otap_convert_type(uint8_t scrat_type);

wp_ScratchpadStatus Proto_otap_convert_status(uint8_t scrat_status);

/**
 * \brief   Handle Get scratchpad status
 * \param   req
 *          Pointer to the request received
 * \param   resp
 *          Pointer to the reponse to send back
 * \return  APP_RES_PROTO_OK if answer is ready to send
 */
app_proto_res_e Proto_otap_handle_get_scratchpad_status(
    wp_GetScratchpadStatusReq * req, wp_GetScratchpadStatusResp * resp);

/**
 * \brief   Handle Upload scratchpad
 * \param   req
 *          Pointer to the request received
 * \param   resp
 *          Pointer to the reponse to send back
 * \return  APP_RES_PROTO_OK if answer is ready to send
 */
app_proto_res_e Proto_otap_handle_upload_scratchpad(wp_UploadScratchpadReq *req,
                                                    wp_UploadScratchpadResp *resp);
                                           
/**
 * \brief   Handle Process scratchpad
 * \param   req
 *          Pointer to the request received
 * \param   resp
 *          Pointer to the reponse to send back
 * \return  APP_RES_PROTO_OK if answer is ready to send
 */
app_proto_res_e Proto_otap_handle_process_scratchpad(wp_ProcessScratchpadReq *req,
                                                     wp_ProcessScratchpadResp *resp);
#endif