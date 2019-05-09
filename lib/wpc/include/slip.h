/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef SLIP_H__
#define SLIP_H__

#include <stdint.h>

// Helper macro to correctly size the encoded buffer
#define RECOMMENDED_BUFFER_SIZE(__buffer_in_len__) ((__buffer_in_len__) *2 + 2)

/**
 * \brief   Decode a slip encoded buffer
 * \parambuffer
 *          buffer address of the encoded buffer.
 *          The same buffer is used to store the decoded buffer
 * \paramlen_in
 *          length of the encoded buffer
 * \returnthe length of the decoded buffer or -1 if buffer cannot be decoded
 * \note    This function is public mainly for testing
 */
int Slip_decode(uint8_t * buffer, uint32_t len_in);

/**
 * \brief   Encode a buffer in slip format
 * \parambuffer_in
 *          the buffer to encode
 * \param   len_in
 *          length of the buffer to encode
 * \parambuffer_out
 *          the address to store the encoded buffer
 * \param   len_out
 *          the length of the provided buffer.
 * \returnthe length of the encoded buffer or -1 if the provided
 *          buffer is too small
 * \noteTo be sure that the buffer is big enough, len_out should
 *          be equal to 2*len_in + 2
 * \note    This function is public mainly for testing
 */
int Slip_encode(uint8_t * buffer_in, uint32_t len_in, uint8_t * buffer_out, uint32_t len_out);

/**
 * \brief   Send a buffer in slip encoding
 * \param   buffer
 *          the buffer to send
 * \param   len
 *          the length of the buffer
 * \return  0 for success, -1 otherwise
 */
int Slip_send_buffer(uint8_t * buffer, uint32_t len);

/**
 * \brief   Get a buffer in slip encoding
 * \param   buffer
 *          the buffer to store data
 * \param   len
 *          length of the provided buffer
 * \param   timeout_ms
 *          the timeout in millisecond to wait for data
 * \return  0 for success, -1 otherwise
 */
int Slip_get_buffer(uint8_t * buffer, uint32_t len, uint16_t timeout_ms);

/**
 * \brief    Function prototype to write data to serial
 * \param    buffer
 *           the buffer to write
 * \param    buffer_size
 *           the buffer size to write
 * \return   the number of bytes written or -1 in case of error
 */
typedef int (*write_f)(const unsigned char * buffer, unsigned int buffer_size);

/**
 * \brief    Function prototype to read single char from serial
 * \param    c
 *           the buffer to store read char
 * \param    timeout_ms
 *           timeout in ms to receive char
 * \return   the number of bytes read, 0 in case of timeout or -1 in case of
 * error
 */
typedef int (*read_f)(unsigned char * c, unsigned int timeout_ms);

/**
 * \brief    Init function for the slip module
 * \param    write
 *           A function to write on the serial line
 * \param    read
 *           A function to read on the serial line
 * \return   0 in case of success, -1 otherwise
 */
int Slip_init(write_f write, read_f read);

#endif
