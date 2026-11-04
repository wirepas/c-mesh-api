/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/*
 * attribute_util.h
 *
 * This file avoid code duplication between csap and msap as
 * attribute handling is done in same manner
 */

#ifndef ATTRIBUTE_UTIL_H_
#define ATTRIBUTE_UTIL_H_

#include <stdint.h>

// Maximum number of bytes in an attribute
// Must be large enough for the biggest attribute (attribute 28,
// C_PROVISIONING_CONFIG_ID, which carries 37 bytes of fixed fields plus a
// variable-length UID of up to 79 bytes). The payload union of wpc_frame_t
// stays representable by its 8-bit payload_length field: this is enforced by
// the static assertion in wpc_types.h.
#define MAX_ATTRIBUTE_SIZE 128

typedef struct __attribute__((__packed__))
{
    uint16_t attribute_id;
    uint8_t attribute_length;
    uint8_t attribute_value[MAX_ATTRIBUTE_SIZE];
} attribute_write_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint16_t attribute_id;
} attribute_read_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint16_t attribute_id;
    uint8_t attribute_length;
    uint8_t attribute_value[MAX_ATTRIBUTE_SIZE];
} attribute_read_conf_pl_t;

/**
 * \brief    Request to write an attribute to the stack
 * \param    primitive_id
 *           Can be a MSAP or CSAP attribute
 * \param    attribute_id
 *           The attribute id to write
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int attribute_write_request(uint8_t primitive_id,
                            uint16_t attribute_id,
                            uint8_t attribute_length,
                            const uint8_t * attribute_value_p);
/**
 * \brief    Request to read an attribute from the stack
 * \param    primitive_id
 *           Can be a MSAP or CSAP attribute
 * \param    attribute_id
 *           The attribute id to read
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to store the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int attribute_read_request(uint8_t primitive_id,
                           uint16_t attribute_id,
                           uint8_t attribute_length,
                           uint8_t * attribute_value_p);
/**
 * \brief    Request to read a variable-length attribute from the stack
 * \param    primitive_id
 *           Can be a MSAP or CSAP attribute
 * \param    attribute_id
 *           The attribute id to read
 * \param    max_length
 *           The size of the buffer pointed by attribute_value_p
 * \param    attribute_value_p
 *           Pointer to store the attribute value
 * \return   negative value if the request fails or if the confirmed
 *           attribute does not fit in max_length bytes,
 *           the confirmed attribute length (0 or more) otherwise.
 *           The value is only copied when it fits the caller's buffer.
 */
int attribute_read_request_vlen(uint8_t primitive_id,
                                uint16_t attribute_id,
                                uint8_t max_length,
                                uint8_t * attribute_value_p);

#endif /* ATTRIBUTE_UTIL_H_ */
