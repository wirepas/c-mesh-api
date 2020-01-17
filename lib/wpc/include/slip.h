/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef SLIP_H__
#define SLIP_H__

#include <stdint.h>
#include <stdlib.h>

/**
 * \brief Maximum number of bytes for \ref Slip_send_buffer()
 *        and \ref Slip_get_buffer()
 */
#define MAX_SLIP_FRAME_SIZE 256

/**
 * \brief   Decode a slip encoded buffer
 * \param   buffer
 *          buffer address of the encoded buffer
 *          The same buffer is used to store the decoded buffer
 * \param   len_in
 *          length of the encoded buffer
 * \return  the length of the decoded buffer or < 0 if buffer cannot be decoded
 * \note    This function is public mainly for testing
 */
int Slip_decode(uint8_t * buffer, size_t len_in);

/**
 * \brief   Encode a buffer in slip format
 * \parambuffer_in
 *          the buffer to encode
 * \param   len_in
 *          length of the buffer to encode
 * \parambuffer_out
 *          the address to store the encoded buffer
 * \param   len_out
 *          the length of the provided buffer
 * \return  the length of the encoded buffer or < 0 if the provided
 *          buffer is too small
 * \note    To be sure that the buffer is big enough, len_out should
 *          be equal to 2*len_in + 2
 * \note    This function is public mainly for testing
 */
int Slip_encode(const uint8_t * buffer_in, size_t len_in, uint8_t * buffer_out, size_t len_out);

/**
 * \brief   Send a buffer in slip encoding
 * \param   buffer
 *          the buffer to send
 * \param   len
 *          length of the buffer to send
 * \return  0 for success, < 0 otherwise
 */
int Slip_send_buffer(const uint8_t * buffer, size_t len);

/**
 * \brief   Get a buffer in slip encoding
 * \param   buffer
 *          the buffer to store data
 * \param   len
 *          length of the provided buffer
 * \param   timeout_ms
 *          the timeout in millisecond to wait for data
 * \return  0 for success, < 0 otherwise
 */
int Slip_get_buffer(uint8_t * buffer, size_t len, unsigned int timeout_ms);

/**
 * \brief    Function prototype to write data to serial
 * \param    buffer
 *           the buffer to write
 * \param    buffer_size
 *           length of the buffer to write
 * \return   the number of bytes written or < 0 in case of error
 */
typedef int (*write_f)(const unsigned char * buffer, size_t len);

/**
 * \brief    Function prototype to read single byte from serial
 * \param    byte_p
 *           the buffer to store read byte
 * \param    timeout_ms
 *           timeout in ms to receive byte
 * \return   the number of bytes read, 0 in case of timeout or < 0 in case of
 *           error
 */
typedef int (*read_f)(unsigned char * byte_p, unsigned int timeout_ms);

/**
 * \brief    Init function for the slip module
 * \param    write
 *           A function to write on the serial line
 * \param    read
 *           A function to read on the serial line
 * \return   0 in case of success, < 0 otherwise
 */
int Slip_init(write_f write, read_f read);

#endif
