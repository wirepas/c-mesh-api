/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef REASSEMBLY_H__
#define REASSEMBLY_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * \brief   Struct representing a fragment
 */
typedef struct {
    uint32_t src_add; //< Source address of the fragmeented packet
    uint16_t packet_id; //< Id of the packet
    size_t size; //< Size of the fragment
    size_t offset; //< Offset of fragment in full message
    bool last_fragment; //< Is it last fragment
    uint8_t * bytes; //< Frgament payload
    unsigned long long timestamp; //< When was the fragment received
} reassembly_fragment_t;

/**
 * \brief   Set maximum duration for fragment
 * \param   duration_s
 *          the maximum duration in seconds to keep fragment from incomplete packets.
 *          Zero equals forever
 * \return  Return code of the operation
 */
void reassembly_set_max_fragment_duration(unsigned int duration_s);

/**
 * \brief   Initialize reassembly module
 */
void reassembly_init();

/**
 * \brief   Check queue emptyness
 * \return  True means that queue is empty, false that queue is most probably not empty
 * \note    This function can be called from any task
 */
bool reassembly_is_queue_empty();

/**
 * \brief   Add fragment to an existing full message
 *          Full message holder will be created if first fragment
 * \param   frag
 *          New fragment
 * \param   full_size_p
 *          Pointer with the full size of the packet. It is set to 0 until
 *          the packet is fully received
 * \return  true if packet is correctly added, false otherwise
 */
bool reassembly_add_fragment(reassembly_fragment_t * frag, size_t * full_size_p);

/**
 * \brief   Get full message
 * \param   src_addr
 *          Source address of the packet
 * \param   packet_id
 *          Id of the packet
 * \param   buffer_p
 *          Pointer for the buffer to strore the message
 * \param   size
 *          [In] Size of the buffer to write packet [Out] real size of the packet
 *          If size is too small to fit the message, the message is not written.
 * \return  true If message was successfully retrieved, fasle otherwise
 * \note    After a successful call, the packet is removed from internal buffer and
 *          cannot be retrieve a second time.
 */
bool reassembly_get_full_message(uint32_t src_addr, uint16_t packet_id, uint8_t * buffer_p, size_t * size);

/**
 * \brief   Clear all the uncomplete fragmented message that have no activity for \ref timeout_s
 *
 */
void reassembly_garbage_collect();

#endif //REASSEMBLY_H__
