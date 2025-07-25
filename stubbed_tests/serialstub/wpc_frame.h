#pragma once

#include <cstdint>
#include <array>

/**
* \brief A Dual-MCU API frame
*/
struct __attribute__((packed)) WpcFrame
{
    uint8_t primitive_id;
    uint8_t frame_id;
    uint8_t payload_length;
    std::array<uint8_t, UINT8_MAX> payload;
    uint16_t crc;
};

