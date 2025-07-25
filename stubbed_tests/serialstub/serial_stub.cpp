#include "serial_stub.h"

std::optional<WpcFrame> SerialStub::ReadFrame(const uint8_t* const buffer, const size_t buffer_size)
{
    WpcFrame decoded = { 0 };
    size_t read_index;
    size_t write_index;
    bool in_escape;
    for (read_index = 0, write_index = 0, in_escape = false; read_index < buffer_size; read_index++)
    {
        const uint8_t byte = buffer[read_index];
        if (byte == Slip::END)
        {
            continue;
        }

        auto decoded_bytes = reinterpret_cast<uint8_t*>(&decoded);

        if (write_index >= sizeof(decoded))
        {
            // error, too big frame
            return std::nullopt;
        }

        if (in_escape)
        {
            if (byte == Slip::ESC_END)
            {
                decoded_bytes[write_index++] = Slip::ESC;
            }
            else if (byte == Slip::ESC_ESC)
            {
                decoded_bytes[write_index++] = Slip::END;
            }
            else
            {
                // error, invalid byte
                return std::nullopt;
            }
            in_escape = false;
        }
        else
        {
            if (byte == Slip::ESC)
            {
                in_escape = true;
            }
            else
            {
                decoded_bytes[write_index++] = byte;
            }
        }
    }

    // CRC probably ends up inside the payload at this point; fix it
    auto wrong_crc_location = reinterpret_cast<uint16_t*>(&decoded.payload[decoded.payload_length]);
    decoded.crc = *wrong_crc_location;
    *wrong_crc_location = 0;

    return decoded;
}

void SerialStub::ReadAndHandleFrame(const uint8_t* const buffer, const size_t buffer_size)
{
    const auto frame = ReadFrame(buffer, buffer_size);
    if (frame.has_value())
    {
        HandleFrame(*frame);
    }
}

void SerialStub::HandleFrame(const WpcFrame& frame)
{
    FrameHandler::ptr handler = nullptr;

    {
        std::lock_guard<std::mutex> lock(message_handlers_mutex);
        if (auto it = message_handlers.find(frame.primitive_id); it != message_handlers.cend())
        {
            handler = it->second;
        }
    }

    if (handler)
    {
        const auto& responses = handler->Handle(frame);
        for (const auto& response : responses)
        {
            AddToOutQueue(response);
        }
    }
}

void SerialStub::AddFrameHandler(const uint8_t primitive_id, FrameHandler::ptr handler)
{
    std::lock_guard<std::mutex> lock(message_handlers_mutex);
    message_handlers[primitive_id] = handler;
}

void SerialStub::ClearFrameHandlers()
{
    std::lock_guard<std::mutex> lock(message_handlers_mutex);
    message_handlers.clear();
}

void SerialStub::AddToOutQueue(const WpcFrame& frame)
{
    out_queue.push_back(Slip::END);

    auto frame_bytes = reinterpret_cast<const uint8_t*>(&frame);

    static_assert(std::numeric_limits<decltype(WpcFrame::payload_length)>::max() <= sizeof(WpcFrame::payload));
    const size_t payload_end_offset = offsetof(WpcFrame, payload) + frame.payload_length;
    for (size_t i = 0; i < payload_end_offset; i++)
    {
        EncodeAndAddToOutQueue(frame_bytes[i]);
    }

    for (size_t i = offsetof(WpcFrame, crc); i < offsetof(WpcFrame, crc) + sizeof(WpcFrame::crc); i++)
    {
        EncodeAndAddToOutQueue(frame_bytes[i]);
    }

    out_queue.push_back(Slip::END);
}

void SerialStub::EncodeAndAddToOutQueue(const uint8_t byte)
{
    if (byte == Slip::END)
    {
        out_queue.push_back(Slip::ESC);
        out_queue.push_back(Slip::ESC_END);
    }
    else if (byte == Slip::ESC)
    {
        out_queue.push_back(Slip::ESC);
        out_queue.push_back(Slip::ESC_END);
    }
    else
    {
        out_queue.push_back(byte);
    }
}

std::optional<uint8_t> SerialStub::PopOutByte()
{
    if (!out_queue.empty())
    {
        const uint8_t byte = out_queue.front();
        out_queue.pop_front();
        return byte;
    }

    return std::nullopt;
}

