/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef UTIL_H__
#define UTIL_H__

/**
 * \brief   Function for encoding a uint16 value in Little endian.
 * \param   value
 *          Value to be encoded.
 * \param   p_encoded_data
 *          Buffer where the encoded data is to be written.
 * \return  Number of bytes written.
 */
static inline uint8_t uint16_encode_le(uint16_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t)((value & 0x00FF) >> 0);
    p_encoded_data[1] = (uint8_t)((value & 0xFF00) >> 8);
    return sizeof(uint16_t);
}

/**
 * \brief   Function for encoding a uint32 value in Little endian.
 * \param   value
 *          Value to be encoded.
 * \param   p_encoded_data
 *          Buffer where the encoded data is to be written.
 * \return  Number of bytes written.
 */
static inline uint8_t uint32_encode_le(uint32_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t)((value & 0x000000FF) >> 0);
    p_encoded_data[1] = (uint8_t)((value & 0x0000FF00) >> 8);
    p_encoded_data[2] = (uint8_t)((value & 0x00FF0000) >> 16);
    p_encoded_data[3] = (uint8_t)((value & 0xFF000000) >> 24);
    return sizeof(uint32_t);
}

/**
 * \brief   Function for decoding a uint16 value form Little endian.
 * \param   p_encoded_data
 *          Buffer where the encoded data is stored.
 * \return  Decoded value.
 */
static inline uint16_t uint16_decode_le(const uint8_t * p_encoded_data)
{
    return ((((uint16_t)((uint8_t *) p_encoded_data)[0])) |
            (((uint16_t)((uint8_t *) p_encoded_data)[1]) << 8));
}

/**
 * \brief   Function for decoding a uint32 value from Little endian
 * \param   p_encoded_data
 *          Buffer where the encoded data is stored.
 * \return  Decoded value.
 */
static inline uint32_t uint32_decode_le(const uint8_t * p_encoded_data)
{
    return ((((uint32_t)((uint8_t *) p_encoded_data)[0]) << 0) |
            (((uint32_t)((uint8_t *) p_encoded_data)[1]) << 8) |
            (((uint32_t)((uint8_t *) p_encoded_data)[2]) << 16) |
            (((uint32_t)((uint8_t *) p_encoded_data)[3]) << 24));
}

#define MAX_INTERNAL_TIME_FOR_FULL_PRECISION (((uint32_t)(-1)) / 1000)

/**
 * \brief   Convert an internal time (1/128s granularity) into ms
 * \param   internal_time
 *          Internal time in 1/128s granularity
 * \return  Equivalent time in ms
 * \note    This conversion is valid only if internal_time is smaller
 *          than 2^32 ms (ie ~50 days)
 */
static inline uint32_t internal_time_to_ms(uint32_t internal_time)
{
    // Check the internal time to give a good conversion and
    // avoid using 64bits computation
    if (internal_time < MAX_INTERNAL_TIME_FOR_FULL_PRECISION)
    {
        return (internal_time * 1000) >> 7;
    }
    else
    {
        // Do the division first (loss of LSB)
        return (internal_time >> 7) * 1000;
    }
}

/**
 * \brief   Convert an internal time (1/128s granularity) into s
 * \param   internal_time
 *          Internal time in 1/128s granularity
 * \return  Equivalent time in ms
 */
static inline uint32_t internal_time_to_s(uint32_t internal_time)
{
    return internal_time >> 7;
}

#define MAX_MS_TIME_FOR_FULL_PRECISION (((uint32_t)(-1)) / 128)

/**
 * \brief   Convert a time in ms to internal time (1/128s granularity)
 * \param   time_in_ms
 *          Time in ms
 * \return  Equivalent time in internal time
 * \note    This conversion is valid only if time_in_ms is smaller
 *          than 2^32 ms (ie ~50 days)
 */
static inline uint32_t ms_to_internal_time(uint32_t time_in_ms)
{
    if (time_in_ms < MAX_MS_TIME_FOR_FULL_PRECISION)
    {
        return (time_in_ms << 7) / 1000;
    }
    else
    {
        return (time_in_ms / 1000) >> 7;
    }
}
#endif
