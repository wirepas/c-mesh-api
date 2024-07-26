/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef COMMON_H_
#define COMMON_H_

#include "generic_message.pb.h"
#include "wpc.h"
#include "wpc_proto.h"

/**
 * \brief   Intialize the common module
 * \return  True if successful, False otherwise
 */
bool Common_init(char * gateway_id,
                 char * gateway_model,
                 char * gateway_version,
                 char * sink_id);

char * Common_get_gateway_id(void);

char * Common_get_gateway_model(void);

char * Common_get_gateway_version(void);

char * Common_get_sink_id(void);

wp_ErrorCode Common_convert_error_code(app_res_e error);

/**
 * \brief   Fill event message header
 * \param   header_p pointer to header to fill
 */
void Common_fill_event_header(wp_EventHeader * header_p);

#endif