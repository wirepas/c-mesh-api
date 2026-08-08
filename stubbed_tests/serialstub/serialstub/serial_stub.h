#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <serialstub/frame_handler.h>
#include <serialstub/wpc_frame.h>

/*
* Thread safety notes:
* - Serial_write and the following Serial_read are always called from the same thread
* - Serial_write and Serial_read calls can be done from different threads (main thread and poll thread)
* - AddFrameHandler is locked because polling thread might be using a frame handler in the background
*/
class SerialStub
{
public:
    void ReadAndHandleFrame(const uint8_t* const buffer, const size_t buffer_size);

    void AddFrameHandler(const uint8_t primitive_id, FrameHandler::ptr handler);
    void ClearFrameHandlers();
    std::optional<uint8_t> PopOutByte();
private:
    struct Slip
    {
        inline static const uint8_t END = 0xC0;
        inline static const uint8_t ESC = 0xDB;
        inline static const uint8_t ESC_ESC = 0xDD;
        inline static const uint8_t ESC_END = 0xDC;
    };

    std::optional<WpcFrame> ReadFrame(const uint8_t* const buffer, const size_t buffer_size);
    void HandleFrame(const WpcFrame& frame);
    void AddToOutQueue(const WpcFrame& frame);
    void EncodeAndAddToOutQueue(const uint8_t byte);

    std::mutex message_handlers_mutex;
    std::unordered_map<uint8_t, FrameHandler::ptr> message_handlers;
    std::deque<uint8_t> out_queue;
};

inline SerialStub serial_stub;

