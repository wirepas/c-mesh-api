#include "crc.h"

uint16_t Crc::FromBuffer(const uint8_t* const buffer, const size_t length)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; i++)
    {
        const uint8_t index = buffer[i] ^ (crc >> 8);
        crc = crc_ccitt_lut[index] ^ (crc << 8);
    }
    return crc;
}

