/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define LOG_MODULE_NAME "SLIP"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
//#define PRINT_BUFFERS
#include "logger.h"

#include "wpc_internal.h"
#include "slip.h"
#include "util.h"

//#define PRINT_RECEIVED_CHAR

// lut table size 512B (256 * 16bit)
static const uint16_t crc_ccitt_lut[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108,
    0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
    0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee,
    0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
    0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d,
    0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5,
    0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
    0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
    0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
    0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13,
    0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
    0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e,
    0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1,
    0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
    0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
    0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
    0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882,
    0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e,
    0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
    0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d,
    0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
    0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

// Special SLIP octet
#define END_SLIP_OCTET 0xC0
#define ESC_SLIP_OCTET 0xDB

// Substitution octet
#define END_SUBS_OCTET 0xDC
#define ESC_SUBS_OCTET 0xDD

// An encoded buffer can be 2 times bigger if all bytes are
// escaped, plus 2 bytes for CRC and 4 bytes for SLIP END symbols
#define MAX_SIZE_ENCODED_BUFFER(__initial_len__) \
    ((__initial_len__ << 1) + 2 + 4)

static write_f write_function = NULL;
static read_f read_function = NULL;

/**
 * Compute the crc of a frame
 */
static uint16_t crc_fromBuffer(uint8_t * buf, uint32_t len)
{
    uint16_t crc = 0xffff;
    uint8_t index;
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        index = buf[i] ^ (crc >> 8);
        crc = crc_ccitt_lut[index] ^ (crc << 8);
    }
    return crc;
}

/**
 * \brief   Convert an escaped frame to a normal frame
 * \param   buffer
 *          buffer of the frame [in] escaped [out] normal
 * \param   len
 *          lenght of the escaped frame
 * \return  lenght of the normal frame
 */
static uint32_t slip_decode_buffer(uint8_t * buffer, uint32_t len)
{
    uint8_t write = 0;
    uint32_t read;
    for (read = 0; read < len; read++)
    {
        uint8_t current = buffer[read];
        if (current == ESC_SLIP_OCTET)
        {
            read++;
            current = (buffer[read] == END_SUBS_OCTET ? END_SLIP_OCTET : ESC_SLIP_OCTET);
        }
        buffer[write++] = current;
    }
    return write;
}

/**
 * \brief   Convert a normal frame to an escaped frame
 * \param   buffer
 *          buffer to encode
 * \param   len
 *          length of the buffer to encode
 * \param   buffer_escaped
 *          buffer of the escaped frame
 * \param   len_buffer
 *          length of the buffer to store the escaped frame
 * \return  length of the escaped frame or -1 if the escaped frame cannot feet
 * the provided buffer
 */
static uint32_t
slip_encode_buffer(uint8_t * buffer, uint32_t len, uint8_t * buffer_escaped, uint32_t len_escaped)
{
    uint8_t write = 0;
    uint32_t read;
    for (read = 0; read < len; read++)
    {
        uint8_t current = buffer[read];
        if (current == END_SLIP_OCTET || current == ESC_SLIP_OCTET)
        {
            // Check for buffer_escaped overflow
            if (write >= len_escaped)
                return WPC_INT_WRONG_BUFFER_SIZE;
            buffer_escaped[write++] = ESC_SLIP_OCTET;
            current = (current == END_SLIP_OCTET ? END_SUBS_OCTET : ESC_SUBS_OCTET);
        }
        // Check for buffer_escaped overflow
        if (write >= len_escaped)
            return WPC_INT_WRONG_BUFFER_SIZE;

        buffer_escaped[write++] = current;
    }
    return write;
}

int Slip_decode(uint8_t * buffer, uint32_t len)
{
    int decoded_len;
    uint16_t crc, crc_from_frame;

    decoded_len = slip_decode_buffer(buffer, len);
    /* Compute the crc with SLIP encoding removed */
    crc = crc_fromBuffer(buffer, decoded_len - 2);
    /* Get crc from frame */
    crc_from_frame = uint16_decode_le((uint8_t *) (buffer + decoded_len - 2));
    /* Compare crc */
    if (crc != crc_from_frame)
    {
        LOG_PRINT_BUFFER(buffer, len);
        LOGE("Wrong crc 0x%04x (computed) vs 0x%04x (received)\n", crc, crc_from_frame);
        return WPC_INT_WRONG_CRC;
    }

    return decoded_len - 2;
}

int Slip_encode(uint8_t * buffer_in, uint32_t len_in, uint8_t * buffer_out, uint32_t len_out)
{
    int total_size;
    int i;

    // Compute the crc
    uint16_t crc = crc_fromBuffer(buffer_in, len_in);

    // Escape the frame without the crc
    total_size = slip_encode_buffer(buffer_in, len_in, buffer_out, len_out);

    // Check that the buffer is big enough
    if (total_size == -1)
    {
        LOGE("Provided buffer in encode is too small\n");
        return WPC_INT_WRONG_BUFFER_SIZE;
    }

    // Add the escaped crc to the end of the buffer in LE
    i = 0;
    while (i < 2)
    {
        uint8_t temp = (crc >> (8 * i)) & 0xff;
        int temp_size;
        temp_size = slip_encode_buffer(&temp, 1, buffer_out + total_size, len_out - total_size);
        if (temp_size == -1)
        {
            LOGE("Provided buffer in encode is too small\n");
            return WPC_INT_WRONG_BUFFER_SIZE;
        }

        total_size += temp_size;
        i++;
    }

    return total_size;
}

int Slip_send_buffer(uint8_t * buffer, uint32_t len)
{
    int size, written_size;

    // Allocate the encoded buffer.
    uint8_t encoded_buffer[MAX_SIZE_ENCODED_BUFFER(len)];

    // Add 3 end symbols at the beginning
    for (int i = 0; i < 3; i++)
        encoded_buffer[i] = END_SLIP_OCTET;

    size = Slip_encode(buffer, len, encoded_buffer + 3, sizeof(encoded_buffer) - 3);

    // Add end symbol at the end
    encoded_buffer[size + 3] = END_SLIP_OCTET;

    LOG_PRINT_BUFFER(encoded_buffer, size + 4);

    written_size = write_function(encoded_buffer, size + 4);
    if (written_size != size + 4)
    {
        LOGE("Not able to write all the encoded packet %d vs %d\n", written_size, size + 4);
        return WPC_INT_GEN_ERROR;
    }

    return 0;
}

int Slip_get_buffer(uint8_t * buffer, uint32_t len, uint16_t timeout_ms)
{
    // Allocate the receiving buffer.
    uint8_t receiving_buffer[MAX_SIZE_ENCODED_BUFFER(len)];
    unsigned char read = 0;
    int size = 0;
    int decoded_size, res;
    bool start_of_frame_detected = false;

    // LOGD("In Slip_get_buffer with timeout = %d\n", timeout_s);
    while (1)
    {
        // Blocking call for timeout_ms
        // In theory, once size is different than 0, timeout
        // should be shortened. But in practice it doesn't really
        // change anything as once first byte is received, others
        // are following.
        res = read_function(&read, timeout_ms);
        if (res == 0)
        {
            LOGW("Timeout to receive frame (size=%d)\n", size);
            return WPC_INT_TIMEOUT_ERROR;
        }
        else if (res < 0 || res > 1)
        {
            LOGE("Problem in getting buffer res = %d\n", res);
        }
        else if (read == END_SLIP_OCTET)
        {
#ifdef PRINT_RECEIVED_CHAR
            LOG_PRINT_BUFFER(&read, 1);
#endif
            if (start_of_frame_detected)
            {
                // End of the frame, exit the loop
                // Bugfix: sometimes 00 and C0 bytes can be sent while switching
                // off the stack. It results in synchronization lost.
                if (size < 4)
                {
                    // It was probably a wrong frame and it is
                    // the start of a frame instead of the end
                    LOGW("Too small packet received (size=%d)\n", size);
                    size = 0;
                    continue;
                }
                else if (receiving_buffer[0] == 0x7F)
                {
                    // Id 0x7f is used for internal debugging
                    // It shouldn't be produced in released builds but
                    // discard them anyway
                    LOGW("Old internal debug print received (size=%d)\n", size);
                    size = 0;
                    start_of_frame_detected = false;
                    continue;
                }
                break;
            }
            else
            {
                // Beginning of the frame
                start_of_frame_detected = true;
            }
        }
        else if (start_of_frame_detected)
        {
#ifdef PRINT_RECEIVED_CHAR
            LOG_PRINT_BUFFER(&read, 1);
#endif
            receiving_buffer[size] = read;
            size++;
            if (size > sizeof(receiving_buffer))
            {
                LOGE("Receiving too much bytes from serial line %d vs %d\n",
                     size,
                     sizeof(receiving_buffer));
                return WPC_INT_GEN_ERROR;
            }
        }
        else
        {
            LOGD("Receiving bytes between messages 0x%02x\n", read);
        }
    }  // End of while(1)

    // Now, decode the frame
    decoded_size = Slip_decode(receiving_buffer, size);
    if (decoded_size > 0 && decoded_size <= len)
    {
        memcpy(buffer, receiving_buffer, decoded_size);
    }
    LOG_PRINT_BUFFER(buffer, decoded_size);

    LOGD("Out of Slip_get_buffer with size = %d\n", decoded_size);
    return decoded_size;
}

int Slip_init(write_f write, read_f read)
{
    if (!write || !read)
        return WPC_INT_WRONG_PARAM_ERROR;

    write_function = write;
    read_function = read;
    return 0;
}
