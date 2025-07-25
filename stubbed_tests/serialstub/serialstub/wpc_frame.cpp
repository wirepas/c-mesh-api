#include "wpc_frame.h"

#include <algorithm>
#include <serialstub/misc/crc.h>

std::optional<WpcFrame> WpcFrame::Create(const uint8_t primitive_id,
                                         const uint8_t frame_id,
                                         const std::span<const uint8_t> payload)
{
    if (payload.size() > sizeof(WpcFrame::payload))
    {
        return std::nullopt;
    }

    WpcFrame frame {
        .primitive_id = primitive_id,
        .frame_id = frame_id,
        .payload_length = static_cast<uint8_t>(payload.size()),
        .payload = {0},
        .crc = 0,
    };

    std::ranges::copy(payload, frame.payload.begin());
    frame.crc = Crc::FromBuffer(reinterpret_cast<const uint8_t*>(&frame),
                                offsetof(WpcFrame, payload) + frame.payload_length);

    return frame;
}
