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

typedef struct __attribute__((__packed__))
{
    uint16_t attribute_id;
    uint8_t attribute_length;
    uint8_t attribute_value[16];
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
    uint8_t attribute_value[16];
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
                            uint8_t * attribute_value_p);
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

#endif /* ATTRIBUTE_UTIL_H_ */
