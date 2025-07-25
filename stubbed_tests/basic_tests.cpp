#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <serial_stub.h>
extern "C" {
#include <wpc.h>
}

using namespace testing;

class StubbedTest : public testing::Test
{
protected:
    class MockFrameHandler : public FrameHandler
    {
    public:
        MOCK_METHOD(std::vector<WpcFrame>, Handle, (const WpcFrame&), (override));
    };

    void SetUp() override
    {
        WPC_initialize("", 0);
    }

    void TearDown() override
    {
        WPC_close();
        // Expectations on mock objects are done when they are deleted here
        serial_stub.ClearFrameHandlers();
    }
};

TEST_F(StubbedTest, exampleVerifyingSentRequest)
{
    auto handler = std::make_shared<MockFrameHandler>();

    const std::array<uint8_t, 16> TEST_KEY = {
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    };

    std::array<uint8_t, UINT8_MAX> EXPECTED_REQUEST_PAYLOAD = {
        13, // Attribute id
        00, 0x10, // Attribute length
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    };

    EXPECT_CALL(*handler, Handle(FieldsAre(0x0D, _, 19, EXPECTED_REQUEST_PAYLOAD, _)))
        .Times(1);
    serial_stub.AddFrameHandler(0x0D, handler);

    WPC_set_cipher_key(TEST_KEY.data());
}

TEST_F(StubbedTest, exampleVerifyingSentRequestWithSavedArgument)
{
    auto handler = std::make_shared<MockFrameHandler>();

    const std::array<uint8_t, 16> TEST_KEY = {
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    };

    std::array<uint8_t, UINT8_MAX> EXPECTED_REQUEST_PAYLOAD = {
        0x0D, 0x00, // Attribute id
        0x10, // Attribute length
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    };

    WpcFrame sentFrame = {};

    EXPECT_CALL(*handler, Handle)
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&sentFrame),
                        Return(std::vector<WpcFrame>())));
    serial_stub.AddFrameHandler(0x0D, handler);

    WPC_set_cipher_key(TEST_KEY.data());

    EXPECT_EQ(sentFrame.primitive_id, 0x0D);
    EXPECT_EQ(sentFrame.payload_length, 19);
    EXPECT_EQ(sentFrame.payload, EXPECTED_REQUEST_PAYLOAD);
}

TEST_F(StubbedTest, exampleUsingSameHandlerForMultiplePrimitives)
{
    auto handler = std::make_shared<MockFrameHandler>();

    EXPECT_CALL(*handler, Handle)
        .Times(2);

    // 0x1B: MSAP-SCRATCHPAD_CLEAR.request
    // 0x38: MSAP-SINK_COST_WRITE.request
    serial_stub.AddFrameHandler(0x1B, handler);
    serial_stub.AddFrameHandler(0x38, handler);

    WPC_clear_local_scratchpad();
    WPC_set_sink_cost(10);
}

