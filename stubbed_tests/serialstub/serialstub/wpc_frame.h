#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <span>

/**
* \brief A Dual-MCU API frame
*/
struct [[gnu::packed]] WpcFrame
{
    uint8_t primitive_id;
    uint8_t frame_id;
    uint8_t payload_length;
    std::array<uint8_t, UINT8_MAX> payload;
    uint16_t crc;

    /**
    * \brief Construct a frame calculating payload length and CRC automatically
    * \return std::nullopt if payload size is too large, std::optional holding
    * the created WpcFrame otherwise.
    */
    static std::optional<WpcFrame> Create(const uint8_t primitive_id,
                                          const uint8_t frame_id,
                                          const std::span<const uint8_t> payload);
};

