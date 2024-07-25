/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PROTO_DATA_H_
#define PROTO_DATA_H_

#include "generic_message.pb.h"
#include "wpc.h"
#include "wpc_proto.h"

/**
 * \brief   Intialize the module in charge of data handling uplink/downlink
 * \return  True if successful, False otherwise
 */
bool Proto_data_init(void);

app_proto_res_e Proto_data_handle_send_data(wp_SendPacketReq *req,
                                            wp_SendPacketResp *resp);

app_proto_res_e Proto_data_register_for_data(onDataRxEvent_cb_f onDataRxEvent_cb);

#endif