/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef CSAP_H_
#define CSAP_H_

#include <stdint.h>
#include "wpc_constants.h"
#include "attribute.h"

/* Attributes ID */
#define C_NODE_ADDRESS_ID 1
#define C_NETWORK_ADDRESS_ID 2
#define C_NETWORK_CHANNEL_ID 3
#define C_NODE_ROLE_ID 4
#define C_MTU_ID 5
#define C_PDU_BUFFER_SIZE_ID 6
#define C_SCRATCHPAD_SEQUENCE_ID 7
#define C_MESH_API_VER_ID 8
#define C_FIRMWARE_MAJOR_ID 9
#define C_FIRMWARE_MINOR_ID 10
#define C_FIRMWARE_MAINT_ID 11
#define C_FIRMWARE_DEV_ID 12
#define C_CIPHER_KEY_ID 13
#define C_AUTH_KEY_ID 14
#define C_CHANNEL_LIM_ID 15
#define C_APP_CONFIG_DATA_SIZE_ID 16
#define C_HW_MAGIC 17
#define C_STACK_PROFILE 18
#define C_OFFLINE_SCAN 20
#define C_CHANNEL_MAP 21
#define C_FEATURE_LOCK_BITS 22
#define C_FEATURE_LOCK_KEY 23

typedef struct __attribute__((__packed__))
{
    uint32_t reset_key;
} csap_factory_reset_req_pl_t;

/**
 * \brief    Request to write a configuration attribute to the stack
 * \param    attribute_id
 *           The attribute id to write
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
static inline int csap_attribute_write_request(uint16_t attribute_id,
                                               uint8_t attribute_length,
                                               uint8_t * attribute_value_p)
{
    return attribute_write_request(CSAP_ATTRIBUTE_WRITE_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

/**
 * \brief    Request to read a configuration attribute from the stack
 * \param    attribute_id
 *           The attribute id to read
 * \param    attribute length
 *           The attribute length
 * \param    attribute_value_p
 *           Pointer to store the attribute value
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
static inline int csap_attribute_read_request(uint16_t attribute_id,
                                              uint8_t attribute_length,
                                              uint8_t * attribute_value_p)
{
    return attribute_read_request(CSAP_ATTRIBUTE_READ_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

/**
 * \brief    Clear all persistent attributes
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int csap_factory_reset_request();

#endif /* CSAP_H_ */
