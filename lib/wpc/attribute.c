/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "attribute"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "attribute.h"
#include "wpc_types.h"
#include "wpc_internal.h"
#include "string.h"

int attribute_write_request(uint8_t primitive_id,
                            uint16_t attribute_id,
                            uint8_t attribute_length,
                            uint8_t * attribute_value_p)
{
    int res;
    wpc_frame_t request, confirm;

    if (attribute_length > 16)
        return -1;

    request.primitive_id = primitive_id;
    uint16_encode_le(attribute_id,
                     (uint8_t *) &(
                         request.payload.attribute_write_request_payload.attribute_id));
    request.payload.attribute_write_request_payload.attribute_length = attribute_length;
    memcpy(request.payload.attribute_write_request_payload.attribute_value,
           attribute_value_p,
           attribute_length);

    request.payload_length = sizeof(attribute_write_req_pl_t) - (16 - attribute_length);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGD("Attribute write result = %d\n",
         confirm.payload.sap_generic_confirm_payload.result);
    return confirm.payload.sap_generic_confirm_payload.result;
}

int attribute_read_request(uint8_t primitive_id,
                           uint16_t attribute_id,
                           uint8_t attribute_length,
                           uint8_t * attribute_value_p)
{
    int res;
    wpc_frame_t request, confirm;

    request.primitive_id = primitive_id;
    uint16_encode_le(attribute_id,
                     (uint8_t *) &(
                         request.payload.attribute_read_request_payload.attribute_id));
    request.payload_length = sizeof(attribute_read_req_pl_t);

    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    LOGD("Attribute Id = %d read result = %d\n",
         confirm.payload.attribute_read_confirm_payload.attribute_id,
         confirm.payload.attribute_read_confirm_payload.result);

    if (confirm.payload.attribute_read_confirm_payload.result == 0)
    {
        if (attribute_length != confirm.payload.attribute_read_confirm_payload.attribute_length)
        {
            LOGE("Attribute read: wrong attribute size\n");
            return -1;
        }
        memcpy(attribute_value_p,
               confirm.payload.attribute_read_confirm_payload.attribute_value,
               attribute_length);
    }

    return confirm.payload.attribute_read_confirm_payload.result;
}
