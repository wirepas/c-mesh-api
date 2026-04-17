#include <gtest/gtest.h>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <vector>

#define _Static_assert static_assert
extern "C"
{
#include "dsap.h"
#include "reassembly.h"
#include "util.h"
#include "wpc_constants.h"
#include "wpc_types.h"
}

namespace
{
struct TxCallbackState
{
    uint16_t pdu_id = 0;
    uint32_t buffering_delay_ms = 0;
    uint8_t result = 0;
    int calls = 0;

    void reset()
    {
        pdu_id = 0;
        buffering_delay_ms = 0;
        result = 0;
        calls = 0;
    }
};

struct SendRequestStub
{
    std::vector<wpc_frame_t> frames;
    std::vector<int> transport_results;
    std::vector<uint8_t> confirm_results;
    uint8_t mtu = 102;
    uint8_t confirm_capacity = 7;

    void reset()
    {
        frames.clear();
        transport_results.clear();
        confirm_results.clear();
        mtu = 102;
        confirm_capacity = 7;
    }
};

SendRequestStub g_stub;
TxCallbackState g_tx_cb;

void on_data_sent(uint16_t pdu_id, uint32_t buffering_delay_ms, uint8_t result)
{
    g_tx_cb.pdu_id = pdu_id;
    g_tx_cb.buffering_delay_ms = buffering_delay_ms;
    g_tx_cb.result = result;
    g_tx_cb.calls++;
}
} // namespace

extern "C" bool Platform_lock_request(void)
{
    return true;
}

extern "C" void Platform_unlock_request(void)
{
}

extern "C" unsigned long long Platform_get_timestamp_ms_monotonic(void)
{
    return 0;
}

extern "C" void Platform_LOG(char level, char * module, char * format, va_list args)
{
    (void) level;
    (void) module;
    (void) format;
    (void) args;
}

extern "C" void Platform_print_buffer(uint8_t * buffer, int size)
{
    (void) buffer;
    (void) size;
}

extern "C" int Platform_set_log_level(const int level)
{
    (void) level;
    return 0;
}

extern "C" int Platform_set_module_log_level(const char * const module_name, const int level)
{
    (void) module_name;
    (void) level;
    return 0;
}

extern "C" int * Platform_get_logging_module_level(const char * const module_name,
                                                   const int default_level)
{
    (void) module_name;
    static int level = default_level;
    level = default_level;
    return &level;
}

extern "C" uint8_t WPC_Int_get_mtu(void)
{
    return g_stub.mtu;
}

extern "C" int WPC_Int_send_request(wpc_frame_t * frame, wpc_frame_t * confirm)
{
    g_stub.frames.push_back(*frame);
    const size_t call_index = g_stub.frames.size() - 1;

    const int transport_result =
        call_index < g_stub.transport_results.size() ? g_stub.transport_results[call_index] : 0;
    if (transport_result < 0)
    {
        return transport_result;
    }

    std::memset(confirm, 0, sizeof(*confirm));
    confirm->payload.dsap_data_tx_confirm_payload.result =
        call_index < g_stub.confirm_results.size() ? g_stub.confirm_results[call_index] : 0;
    confirm->payload.dsap_data_tx_confirm_payload.capacity = g_stub.confirm_capacity;
    return transport_result;
}

extern "C" bool reassembly_add_fragment(reassembly_fragment_t * frag, size_t * full_size_p)
{
    (void) frag;
    *full_size_p = 0;
    return false;
}

extern "C" bool reassembly_get_full_message(uint32_t src_add,
                                            uint16_t packet_id,
                                            uint8_t * buffer_p,
                                            size_t * size)
{
    (void) src_add;
    (void) packet_id;
    (void) buffer_p;
    (void) size;
    return false;
}

extern "C" void reassembly_garbage_collect(void)
{
}

extern "C" void reassembly_set_max_fragment_duration(unsigned int fragment_max_duration_s)
{
    (void) fragment_max_duration_s;
}

class SsrRoutingUnitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        g_stub.reset();
        g_tx_cb.reset();
        dsap_init();
    }
};

TEST_F(SsrRoutingUnitTest, LegacySingleFrameRequestKeepsTraditionalPrimitive)
{
    const uint8_t payload[] = { 0x10, 0x20, 0x30 };

    ASSERT_EQ(dsap_data_tx_request(payload,
                                   sizeof(payload),
                                   0x1234,
                                   0x01020304,
                                   1,
                                   5,
                                   7,
                                   nullptr,
                                   128,
                                   false,
                                   9),
              0);

    ASSERT_EQ(g_stub.frames.size(), 1u);
    const wpc_frame_t & frame = g_stub.frames.front();
    EXPECT_EQ(frame.primitive_id, DSAP_DATA_TX_TT_REQUEST);
    EXPECT_EQ(frame.payload_length,
              sizeof(dsap_data_tx_tt_req_pl_t) - (MAX_APDU_DSAP_SIZE - sizeof(payload)));

    const dsap_data_tx_tt_req_pl_t & request = frame.payload.dsap_data_tx_tt_request_payload;
    EXPECT_EQ(request.pdu_id, 0x1234u);
    EXPECT_EQ(request.dest_add, 0x01020304u);
    EXPECT_EQ(request.qos, 1u);
    EXPECT_EQ(request.src_endpoint, 5u);
    EXPECT_EQ(request.dest_endpoint, 7u);
    EXPECT_EQ(request.tx_options, static_cast<uint8_t>(9u << 2));
    EXPECT_EQ(request.buffering_delay, 128u);
    ASSERT_EQ(request.apdu_length, sizeof(payload));
    EXPECT_EQ(std::memcmp(request.apdu, payload, sizeof(payload)), 0);
}

TEST_F(SsrRoutingUnitTest, SsrSingleFrameRequestBuildsExpectedPayloadAndRegistersCallback)
{
    const uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    const uint32_t hops[] = { 0x11111111, 0x22222222 };

    ASSERT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       0xCAFE,
                                       0x01020304,
                                       1,
                                       8,
                                       9,
                                       on_data_sent,
                                       128,
                                       true,
                                       0x1F,
                                       2,
                                       hops),
              0);

    ASSERT_EQ(g_stub.frames.size(), 1u);
    const wpc_frame_t & frame = g_stub.frames.front();
    EXPECT_EQ(frame.primitive_id, DSAP_DATA_TX_SSR_REQUEST);
    EXPECT_EQ(frame.payload_length,
              sizeof(dsap_data_tx_ssr_req_pl_t)
                  - (SSR_MAX_HOPS - 2) * sizeof(uint32_t)
                  - (MAX_APDU_DSAP_SIZE - sizeof(payload)));

    const dsap_data_tx_ssr_req_pl_t & request = frame.payload.dsap_data_tx_ssr_request_payload;
    EXPECT_EQ(request.pdu_id, 0xCAFEu);
    EXPECT_EQ(request.dest_add, 0x01020304u);
    EXPECT_EQ(request.qos, 1u);
    EXPECT_EQ(request.src_endpoint, 8u);
    EXPECT_EQ(request.dest_endpoint, 9u);
    EXPECT_EQ(request.tx_options, 0x3Fu);
    EXPECT_EQ(request.buffering_delay, 128u);
    EXPECT_EQ(request.hop_count, 2u);
    EXPECT_EQ(request.hops[0], hops[0]);
    EXPECT_EQ(request.hops[1], hops[1]);
    ASSERT_EQ(request.apdu_length, sizeof(payload));
    EXPECT_EQ(std::memcmp(request.apdu, payload, sizeof(payload)), 0);

    dsap_data_tx_ind_pl_t indication = {};
    indication.pdu_id = 0xCAFE;
    indication.buffering_delay = 128;
    indication.result = 1;
    dsap_data_tx_indication_handler(&indication);

    EXPECT_EQ(g_tx_cb.calls, 1);
    EXPECT_EQ(g_tx_cb.pdu_id, 0xCAFEu);
    EXPECT_EQ(g_tx_cb.buffering_delay_ms, 1000u);
    EXPECT_EQ(g_tx_cb.result, 1u);
}

TEST_F(SsrRoutingUnitTest, SsrRequestRejectsPacketsLargerThanMaximumFrameSize)
{
    std::vector<uint8_t> payload(MAX_FULL_PACKET_SIZE + 1, 0x55);
    const uint32_t hop = 0xABCDEF01;

    EXPECT_EQ(dsap_data_tx_ssr_request(payload.data(),
                                       payload.size(),
                                       1,
                                       2,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       0,
                                       1,
                                       &hop),
              6);
    EXPECT_TRUE(g_stub.frames.empty());
}

TEST_F(SsrRoutingUnitTest, SsrRequestRejectsInvalidHopArguments)
{
    const uint8_t payload[] = { 0x01 };
    const uint32_t hops[SSR_MAX_HOPS + 1] = {};

    EXPECT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       1,
                                       2,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       0,
                                       SSR_MAX_HOPS + 1,
                                       hops),
              6);

    EXPECT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       1,
                                       2,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       0,
                                       1,
                                       nullptr),
              6);
    EXPECT_TRUE(g_stub.frames.empty());
}

TEST_F(SsrRoutingUnitTest, SingleFrameTransportFailureIsPropagated)
{
    const uint8_t payload[] = { 0x01, 0x02 };
    const uint32_t hop = 0x12345678u;
    g_stub.transport_results = { -7 };

    EXPECT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       1,
                                       2,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       0,
                                       1,
                                       &hop),
              -7);
}

TEST_F(SsrRoutingUnitTest, LegacyFragmentedRequestUsesTraditionalFragmentPrimitive)
{
    const uint8_t payload[] = { 1, 2, 3, 4, 5 };
    g_stub.mtu = 3;

    ASSERT_EQ(dsap_data_tx_request(payload,
                                   sizeof(payload),
                                   0x0102,
                                   0x55667788,
                                   0,
                                   3,
                                   4,
                                   on_data_sent,
                                   0,
                                   false,
                                   6),
              0);

    ASSERT_EQ(g_stub.frames.size(), 2u);
    EXPECT_EQ(g_stub.frames[0].primitive_id, DSAP_DATA_TX_FRAG_REQUEST);
    EXPECT_EQ(g_stub.frames[1].primitive_id, DSAP_DATA_TX_FRAG_REQUEST);

    const dsap_data_tx_frag_req_pl_t & first =
        g_stub.frames[0].payload.dsap_data_tx_frag_request_payload;
    const dsap_data_tx_frag_req_pl_t & last =
        g_stub.frames[1].payload.dsap_data_tx_frag_request_payload;

    EXPECT_EQ(first.tx_options, static_cast<uint8_t>(6u << 2));
    EXPECT_EQ(first.apdu_length, 3u);
    EXPECT_EQ(first.fragment_offset_flag & DSAP_FRAG_LAST_FLAG_MASK, 0u);

    EXPECT_EQ(last.tx_options, static_cast<uint8_t>(1u | (6u << 2)));
    EXPECT_EQ(last.apdu_length, 2u);
    EXPECT_EQ(last.fragment_offset_flag & DSAP_FRAG_LAST_FLAG_MASK, DSAP_FRAG_LAST_FLAG_MASK);
    EXPECT_EQ(last.fragment_offset_flag & DSAP_FRAG_LENGTH_MASK, 3u);
}

TEST_F(SsrRoutingUnitTest, FragmentedSsrRequestBuildsFragmentsAndKeepsCallbackOnlyOnLastFragment)
{
    const uint8_t payload[] = { 0, 1, 2, 3, 4, 5, 6 };
    const uint32_t hop = 0x01020304;
    g_stub.mtu = 4;

    ASSERT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       0x0A0B,
                                       0xDEADBEEF,
                                       1,
                                       6,
                                       7,
                                       on_data_sent,
                                       0,
                                       true,
                                       3,
                                       1,
                                       &hop),
              0);

    ASSERT_EQ(g_stub.frames.size(), 2u);
    EXPECT_EQ(g_stub.frames[0].primitive_id, DSAP_DATA_TX_SSR_FRAG_REQUEST);
    EXPECT_EQ(g_stub.frames[1].primitive_id, DSAP_DATA_TX_SSR_FRAG_REQUEST);

    const dsap_data_tx_ssr_frag_req_pl_t & first =
        g_stub.frames[0].payload.dsap_data_tx_ssr_frag_request_payload;
    const dsap_data_tx_ssr_frag_req_pl_t & last =
        g_stub.frames[1].payload.dsap_data_tx_ssr_frag_request_payload;

    EXPECT_EQ(first.tx_options, static_cast<uint8_t>(0x02u | (3u << 2)));
    EXPECT_EQ(first.hop_count, 1u);
    EXPECT_EQ(first.hops[0], hop);
    EXPECT_EQ(first.apdu_length, 4u);
    EXPECT_EQ(first.fragment_offset_flag & DSAP_FRAG_LAST_FLAG_MASK, 0u);

    EXPECT_EQ(last.tx_options, static_cast<uint8_t>(0x03u | (3u << 2)));
    EXPECT_EQ(last.hop_count, 1u);
    EXPECT_EQ(last.hops[0], hop);
    EXPECT_EQ(last.apdu_length, 3u);
    EXPECT_EQ(last.fragment_offset_flag & DSAP_FRAG_LAST_FLAG_MASK, DSAP_FRAG_LAST_FLAG_MASK);
    EXPECT_EQ(last.fragment_offset_flag & DSAP_FRAG_LENGTH_MASK, 4u);
}

TEST_F(SsrRoutingUnitTest, FragmentedSsrRequestStopsOnTransportFailure)
{
    const uint8_t payload[] = { 1, 2, 3, 4, 5, 6 };
    const uint32_t hop = 0x01020304;
    g_stub.mtu = 3;
    g_stub.transport_results = { 0, -9 };

    EXPECT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       0xAAAA,
                                       0x01020304,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       1,
                                       1,
                                       &hop),
              -9);
    EXPECT_EQ(g_stub.frames.size(), 2u);
}

TEST_F(SsrRoutingUnitTest, FragmentedSsrRequestStopsOnStackRefusal)
{
    const uint8_t payload[] = { 1, 2, 3, 4, 5, 6 };
    const uint32_t hop = 0x01020304;
    g_stub.mtu = 3;
    g_stub.confirm_results = { 0, 5 };

    EXPECT_EQ(dsap_data_tx_ssr_request(payload,
                                       sizeof(payload),
                                       0xBBBB,
                                       0x01020304,
                                       0,
                                       1,
                                       2,
                                       nullptr,
                                       0,
                                       false,
                                       1,
                                       1,
                                       &hop),
              5);
    EXPECT_EQ(g_stub.frames.size(), 2u);
}
