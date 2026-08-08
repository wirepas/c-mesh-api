#include <semaphore>
#include <chrono>
#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <serialstub/serial_stub.h>
#include <serialstub/misc/crc.h>
#include <utils/data_structures.h>
#include <utils/queued_ul_data_handler.h>
extern "C" {
#include <wpc.h>
    #define LOG_MODULE_NAME (char*) "test"
    #include <logger.h>
}

using namespace testing;
using namespace fuzztest;

constexpr uint32_t max_supported_travel_time = (std::numeric_limits<uint32_t>::max() / 1000) - 1;

// Not completely valid, since the 2 bit qos part in qos_hop_count can be 0
// or 1. StructOf didn't play well with bitfields so qos and hop count are not
// separated. Only check first bit for qos when doing assertions.
auto ValidSingleDataRx() {
    return StructOf<DataRxIndication>(
        InRange<uint8_t>(0, 0),                          // indication_status
        Arbitrary<uint32_t>(),                           // src_add
        Arbitrary<uint8_t>(),                            // src_endpoint
        Arbitrary<uint32_t>(),                           // dest_add
        Arbitrary<uint8_t>(),                            // dest_endpoint
        Arbitrary<uint8_t>(),                            // qos_hop_count
        InRange<uint32_t>(0, max_supported_travel_time), // travel_time
        InRange<uint8_t>(0, 102),                        // apdu_length
        Arbitrary<std::array<uint8_t, 102>>()            // apdu
    );
}

class FuzzUlDataTest
{
public:
    static bool onDataReceived(const uint8_t* bytes,
                               size_t num_bytes,
                               app_addr_t src_addr,
                               app_addr_t dst_addr,
                               app_qos_e qos,
                               uint8_t src_ep,
                               uint8_t dst_ep,
                               uint32_t travel_time,
                               uint8_t hop_count,
                               unsigned long long timestamp_ms_epoch)
    {
        last_callback_args = {
            .bytes { bytes, bytes + num_bytes },
            .src_addr = src_addr,
            .dst_addr = dst_addr,
            .qos = qos,
            .src_ep = src_ep,
            .dst_ep = dst_ep,
            .travel_time = travel_time,
            .hop_count = hop_count,
            .timestamp_ms_epoch = timestamp_ms_epoch,
        };
        callback_called.release();

        return true;
    }

    FuzzUlDataTest()
    {
        Platform_set_log_level(NO_LOG_LEVEL);
        WPC_initialize("", 0);

        handler = std::make_shared<QueuedUlDataHandler>();
        WPC_register_for_data(onDataReceived);
        serial_stub.AddFrameHandler(0x04, handler);
    }

    ~FuzzUlDataTest()
    {
        WPC_close();
        serial_stub.ClearFrameHandlers();
    }

    void TestSingleValidUlMessage(const DataRxIndication& dataRx)
    {
        handler->AddToSendQueue(dataRx);

        const uint32_t EXPECTED_TRAVEL_TIME = static_cast<uint64_t>(dataRx.travel_time) * 1000 / 128;
        const app_qos_e EXPECTED_QOS = static_cast<app_qos_e>(dataRx.qos_hop_count & 0b1);
        const uint8_t EXPECTED_HOP_COUNT = dataRx.qos_hop_count >> 2;

        ASSERT_TRUE(callback_called.try_acquire_for(std::chrono::seconds(2))) <<
            "onDataReceived callback wasn't called within the timeout.";
        ASSERT_TRUE(last_callback_args.has_value());
        const auto cb_args = last_callback_args.value();
        last_callback_args.reset();

        ASSERT_EQ(cb_args.bytes.size(), dataRx.apdu_length);
        ASSERT_EQ(cb_args.src_addr, dataRx.src_add);
        ASSERT_EQ(cb_args.dst_addr, dataRx.dest_add);
        ASSERT_EQ(cb_args.src_ep, dataRx.src_endpoint);
        ASSERT_EQ(cb_args.dst_ep, dataRx.dest_endpoint);
        ASSERT_EQ(cb_args.travel_time, EXPECTED_TRAVEL_TIME);
        ASSERT_EQ(cb_args.hop_count, EXPECTED_HOP_COUNT);
        ASSERT_EQ(cb_args.qos, EXPECTED_QOS);
    }

    std::shared_ptr<QueuedUlDataHandler> handler;
    inline static std::binary_semaphore callback_called {0};
    inline static std::optional<OnDataReceivedArgs> last_callback_args;
};


FUZZ_TEST_F(FuzzUlDataTest, TestSingleValidUlMessage)
    .WithDomains(ValidSingleDataRx());

