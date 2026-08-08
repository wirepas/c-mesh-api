#pragma once

#include <deque>
#include <serialstub/frame_handler.h>
#include <utils/data_structures.h>

/**
 * \brief FrameHandler for testing uplink data callbacks
 *
 * It can be registered for MSAP-INDICATION_POLL.request
 * Uplink data packet(s) can be added to the queue via AddToSendQueue().
 * Handle() function returns all of the data packets in the queue and clears the
 * queue.
 */
class QueuedUlDataHandler : public FrameHandler
{
public:
    std::vector<WpcFrame> Handle(const WpcFrame& frame) override;
    void AddToSendQueue(const DataRxIndication& rxFrame);
private:
    WpcFrame getConfirm(const uint8_t frame_id, bool hasIndication);
    WpcFrame getDataRxInd(const DataRxIndication& rx_frame, const uint8_t frame_id);
    std::mutex to_send_mutex;
    std::deque<DataRxIndication> to_send;
};

