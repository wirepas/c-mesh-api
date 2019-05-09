/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "csap"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "wpc_types.h"
#include "wpc_internal.h"
#include "util.h"

#include "string.h"

static const uint8_t reset_key_le[4] = {0x44, 0x6f, 0x49, 0x74};  // DoIt in ascii in LE

int csap_factory_reset_request()
{
    wpc_frame_t request, confirm;
    int res;

    request.primitive_id = CSAP_FACTORY_RESET_REQUEST;
    memcpy((uint8_t *) &request.payload.csap_factory_reset_request_payload,
           reset_key_le,
           sizeof(reset_key_le));
    request.payload_length = sizeof(csap_factory_reset_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGI("Reset request result = 0x%02x\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}
