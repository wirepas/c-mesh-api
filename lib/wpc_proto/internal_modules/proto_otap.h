/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PROTO_OTAP_H_
#define PROTO_OTAP_H_

#include "otap_message.pb.h"
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