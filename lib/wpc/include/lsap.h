/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef LSAP_H_
#define LSAP_H_

#include <stdint.h>
#include "wpc_constants.h"
#include "attribute.h"

/* Attributes ID */
#define L_WAKEUP_TIME 1


/**
 * \brief    Request to write a local attribute
 * \param    attribute_id
 *           The attribute id to write
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
static inline int lsap_attribute_write_request(uint16_t attribute_id,
                                               uint8_t attribute_length,
                                               uint8_t * attribute_value_p)
{
    return attribute_write_request(LSAP_ATTRIBUTE_WRITE_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

/**
 * \brief    Request to read a local attribute
 * \param    attribute_id
 *           The attribute id to read
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to store the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
static inline int lsap_attribute_read_request(uint16_t attribute_id,
                                              uint8_t attribute_length,
                                              uint8_t * attribute_value_p)
{
    return attribute_read_request(LSAP_ATTRIBUTE_READ_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

#endif /* LSAP_H_ */
