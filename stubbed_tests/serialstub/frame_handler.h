#pragma once

#include <memory>
#include <vector>
#include <wpc_frame.h>

class FrameHandler
{
public:
    using ptr = std::shared_ptr<FrameHandler>;

    /**
    * \brief Handle a Dual-MCU API frame
    *
    * Handles a request frame and returns response frames. As an example,
    * multiple responses to a single request are possible when responding to
    * poll requests.
    *
    * In this context, a "request" is a frame sent by the application and a
    * "response" is a frame sent by the stub back to the application.
    *
    * In Dual-MCU API specification, "request" messages are the following:
    * - request
    * - response
    * And the "response" messages are the following:
    * - confirm
    * - indication
    *
    * \param  frame
    *         Request frame to be handled
    * \return Response frames
    */
    virtual std::vector<WpcFrame> Handle(const WpcFrame& frame) = 0;
};

