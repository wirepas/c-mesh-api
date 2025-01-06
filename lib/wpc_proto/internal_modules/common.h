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

/* Return the size of a struct member */
#define member_size(type, member) (sizeof( ((type *)0)->member ))

/* Set the max of strings stored (including), should match values defined in proto files */
#define GATEWAY_ID_MAX_SIZE 32
#define GATEWAY_MODEL_MAX_SIZE 64
#define GATEWAY_VERSION_MAX_SIZE 32
#define SINK_ID_MAX_SIZE 16

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
 * \param   has_sink_id boolean indicating if sink_id should be included in header
 */
void Common_fill_event_header(wp_EventHeader * header_p, bool has_sink_id);

/**
 * \brief   Fill response header
 * \param   header_p
 *          pointer to header to fill
 * \param   req_id
 *          id of the request
 * \param   res
 *          result of the request
 */
void Common_Fill_response_header(wp_ResponseHeader * header_p,
                                 uint64_t req_id,
                                 wp_ErrorCode res);

#endif