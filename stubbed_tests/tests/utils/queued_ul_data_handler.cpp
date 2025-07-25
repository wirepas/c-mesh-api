#include "queued_ul_data_handler.h"

#include <cstring>
#include <serialstub/misc/crc.h>

std::vector<WpcFrame> QueuedUlDataHandler::Handle(const WpcFrame& frame)
{
    std::vector<WpcFrame> responses;

    std::lock_guard<std::mutex> lock {to_send_mutex};
    responses.emplace_back(getConfirm(frame.frame_id, !to_send.empty()));

    for (const auto& rxFrame : to_send)
    {
        responses.emplace_back(getDataRxInd(rxFrame, frame.frame_id));
    }
    to_send.clear();

    return responses;
}

WpcFrame QueuedUlDataHandler::getConfirm(const uint8_t frame_id, bool hasIndication)
{
    const std::array<uint8_t, 1> payload {hasIndication};
    return *WpcFrame::Create(0x84, frame_id, payload);
}

WpcFrame QueuedUlDataHandler::getDataRxInd(const DataRxIndication& rx_frame, const uint8_t frame_id)
{
    // Difference from max payload size
    const auto size_difference = rx_frame.apdu.max_size() - rx_frame.apdu_length;
    const std::span<const uint8_t> data {reinterpret_cast<const uint8_t*>(&rx_frame),
                                         sizeof(DataRxIndication) - size_difference};
    return *WpcFrame::Create(0x03, frame_id, data);
}

void QueuedUlDataHandler::AddToSendQueue(const DataRxIndication& rxFrame)
{
    std::lock_guard<std::mutex> lock {to_send_mutex};
    to_send.emplace_back(rxFrame);
}

