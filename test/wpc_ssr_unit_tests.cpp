#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <vector>

extern "C"
{
#include "attribute.h"
#include "csap.h"
#include "dsap.h"
#include "util.h"
#include "wpc.h"
#include "wpc_constants.h"
}

namespace
{
enum class Event
{
    SsrDeinit,
    IntClose,
};

struct DsTxCall
{
    std::vector<uint8_t> bytes;
    size_t len = 0;
    uint16_t pdu_id = 0;
    uint32_t dest_add = 0;
    uint8_t qos = 0;
    uint8_t src_ep = 0;
    uint8_t dest_ep = 0;
    onDataSent_cb_f on_data_sent_cb = nullptr;
    uint32_t buffering_delay = 0;
    bool is_unack_csma_ca = false;
    uint8_t hop_limit = 0;
    uint8_t hop_count = 0;
    std::vector<uint32_t> hops;
};

struct WpcStubState
{
    int int_initialize_result = 0;
    int int_initialize_calls = 0;
    int int_set_mtu_calls = 0;
    int int_close_calls = 0;
    int ssr_init_calls = 0;
    int ssr_deinit_calls = 0;
    int ssr_clear_sink_address_calls = 0;
    int ssr_reset_routes_calls = 0;
    int ssr_lock_calls = 0;
    int ssr_unlock_calls = 0;
    bool ssr_lock_result = true;
    std::vector<Event> events;
    std::vector<uint32_t> sink_addresses;
    int msap_stack_stop_result = 0;
    int msap_stack_status_read_result = 0;
    uint8_t stack_status = 0;
    std::vector<bool> poll_request_states;
    unsigned long long monotonic_ms = 0;
    int attribute_read_result = 0;
    int attribute_write_result = 0;
    int node_address_read_result = 0;
    int node_address_write_result = 0;
    int role_read_result = 0;
    int role_write_result = 0;
    int node_address_read_calls = 0;
    int role_read_calls = 0;
    app_addr_t node_address = 0;
    app_role_t role = APP_ROLE_SINK;
    uint8_t last_read_primitive = 0;
    uint16_t last_read_attribute_id = 0;
    uint8_t last_read_attribute_length = 0;
    uint8_t last_write_primitive = 0;
    uint16_t last_write_attribute_id = 0;
    uint8_t last_write_attribute_length = 0;
    std::array<uint8_t, MAX_ATTRIBUTE_SIZE> last_write_value = {};
    std::array<uint8_t, MAX_ATTRIBUTE_SIZE> attribute_read_value = {};
    bool ssr_has_route = false;
    uint32_t ssr_first_hop = 0;
    int ssr_lookup_calls = 0;
    uint32_t last_ssr_lookup_dest = 0;
    int dsap_result = 0;
    int dsap_ssr_result = 0;
    bool dsap_called = false;
    bool dsap_ssr_called = false;
    DsTxCall last_dsap_call;
    DsTxCall last_dsap_ssr_call;

    void reset()
    {
        int_initialize_result = 0;
        int_initialize_calls = 0;
        int_set_mtu_calls = 0;
        int_close_calls = 0;
        ssr_init_calls = 0;
        ssr_deinit_calls = 0;
        ssr_clear_sink_address_calls = 0;
        ssr_reset_routes_calls = 0;
        ssr_lock_calls = 0;
        ssr_unlock_calls = 0;
        ssr_lock_result = true;
        events.clear();
        sink_addresses.clear();
        msap_stack_stop_result = 0;
        msap_stack_status_read_result = 0;
        stack_status = 0;
        poll_request_states.clear();
        monotonic_ms = 0;
        attribute_read_result = 0;
        attribute_write_result = 0;
        node_address_read_result = 0;
        node_address_write_result = 0;
        role_read_result = 0;
        role_write_result = 0;
        node_address_read_calls = 0;
        role_read_calls = 0;
        node_address = 0;
        role = APP_ROLE_SINK;
        last_read_primitive = 0;
        last_read_attribute_id = 0;
        last_read_attribute_length = 0;
        last_write_primitive = 0;
        last_write_attribute_id = 0;
        last_write_attribute_length = 0;
        last_write_value.fill(0);
        attribute_read_value.fill(0);
        ssr_has_route = false;
        ssr_first_hop = 0;
        ssr_lookup_calls = 0;
        last_ssr_lookup_dest = 0;
        dsap_result = 0;
        dsap_ssr_result = 0;
        dsap_called = false;
        dsap_ssr_called = false;
        last_dsap_call = {};
        last_dsap_ssr_call = {};
    }
};

WpcStubState g_stub;

DsTxCall make_tx_call(const uint8_t * bytes,
                      size_t len,
                      uint16_t pdu_id,
                      uint32_t dest_add,
                      uint8_t qos,
                      uint8_t src_ep,
                      uint8_t dest_ep,
                      onDataSent_cb_f on_data_sent_cb,
                      uint32_t buffering_delay,
                      bool is_unack_csma_ca,
                      uint8_t hop_limit,
                      uint8_t hop_count,
                      const uint32_t * hops)
{
    DsTxCall call;
    call.bytes.assign(bytes, bytes + len);
    call.len = len;
    call.pdu_id = pdu_id;
    call.dest_add = dest_add;
    call.qos = qos;
    call.src_ep = src_ep;
    call.dest_ep = dest_ep;
    call.on_data_sent_cb = on_data_sent_cb;
    call.buffering_delay = buffering_delay;
    call.is_unack_csma_ca = is_unack_csma_ca;
    call.hop_limit = hop_limit;
    call.hop_count = hop_count;
    if (hop_count != 0 && hops != nullptr)
    {
        call.hops.assign(hops, hops + hop_count);
    }
    return call;
}
} // namespace

extern "C" int WPC_Int_initialize(const char * port_name, unsigned long bitrate)
{
    (void) port_name;
    (void) bitrate;
    g_stub.int_initialize_calls++;
    return g_stub.int_initialize_result;
}

extern "C" void WPC_Int_set_mtu(void)
{
    g_stub.int_set_mtu_calls++;
}

extern "C" void WPC_Int_close(void)
{
    g_stub.int_close_calls++;
    g_stub.events.push_back(Event::IntClose);
}

extern "C" void ssr_init(void)
{
    g_stub.ssr_init_calls++;
}

extern "C" void ssr_deinit(void)
{
    g_stub.ssr_deinit_calls++;
    g_stub.events.push_back(Event::SsrDeinit);
}

extern "C" void ssr_set_sink_address(uint32_t sink_address)
{
    g_stub.sink_addresses.push_back(sink_address);
}

extern "C" void ssr_set_sink_address_locked(uint32_t sink_address)
{
    g_stub.sink_addresses.push_back(sink_address);
}

extern "C" void ssr_clear_sink_address(void)
{
    g_stub.ssr_clear_sink_address_calls++;
}

extern "C" void ssr_clear_sink_address_locked(void)
{
    g_stub.ssr_clear_sink_address_calls++;
}

extern "C" void ssr_reset_routes(void)
{
    g_stub.ssr_reset_routes_calls++;
}

extern "C" void ssr_reset_routes_locked(void)
{
    g_stub.ssr_reset_routes_calls++;
}

extern "C" bool ssr_get_first_hop(uint32_t dest, uint32_t * first_hop_id)
{
    g_stub.ssr_lookup_calls++;
    g_stub.last_ssr_lookup_dest = dest;
    if (!g_stub.ssr_has_route)
    {
        *first_hop_id = 0;
        return false;
    }

    *first_hop_id = g_stub.ssr_first_hop;
    return true;
}

extern "C" bool ssr_get_first_hop_locked(uint32_t dest, uint32_t * first_hop_id)
{
    g_stub.ssr_lookup_calls++;
    g_stub.last_ssr_lookup_dest = dest;
    if (!g_stub.ssr_has_route)
    {
        *first_hop_id = 0;
        return false;
    }

    *first_hop_id = g_stub.ssr_first_hop;
    return true;
}

extern "C" bool Platform_lock_ssr(void)
{
    g_stub.ssr_lock_calls++;
    return g_stub.ssr_lock_result;
}

extern "C" void Platform_unlock_ssr(void)
{
    g_stub.ssr_unlock_calls++;
}

extern "C" int dsap_data_tx_request(const uint8_t * buffer,
                                    size_t len,
                                    uint16_t pdu_id,
                                    uint32_t dest_add,
                                    uint8_t qos,
                                    uint8_t src_ep,
                                    uint8_t dest_ep,
                                    onDataSent_cb_f on_data_sent_cb,
                                    uint32_t buffering_delay,
                                    bool is_unack_csma_ca,
                                    uint8_t hop_limit)
{
    g_stub.dsap_called = true;
    g_stub.last_dsap_call = make_tx_call(buffer,
                                         len,
                                         pdu_id,
                                         dest_add,
                                         qos,
                                         src_ep,
                                         dest_ep,
                                         on_data_sent_cb,
                                         buffering_delay,
                                         is_unack_csma_ca,
                                         hop_limit,
                                         0,
                                         nullptr);
    return g_stub.dsap_result;
}

extern "C" int dsap_data_tx_ssr_request(const uint8_t * buffer,
                                        size_t len,
                                        uint16_t pdu_id,
                                        uint32_t dest_add,
                                        uint8_t qos,
                                        uint8_t src_ep,
                                        uint8_t dest_ep,
                                        onDataSent_cb_f on_data_sent_cb,
                                        uint32_t buffering_delay,
                                        bool is_unack_csma_ca,
                                        uint8_t hop_limit,
                                        uint8_t hop_count,
                                        const uint32_t * hops)
{
    g_stub.dsap_ssr_called = true;
    g_stub.last_dsap_ssr_call = make_tx_call(buffer,
                                             len,
                                             pdu_id,
                                             dest_add,
                                             qos,
                                             src_ep,
                                             dest_ep,
                                             on_data_sent_cb,
                                             buffering_delay,
                                             is_unack_csma_ca,
                                             hop_limit,
                                             hop_count,
                                             hops);
    return g_stub.dsap_ssr_result;
}

extern "C" int attribute_read_request(uint8_t primitive_id,
                                      uint16_t attribute_id,
                                      uint8_t attribute_length,
                                      uint8_t * attribute_value_p)
{
    g_stub.last_read_primitive = primitive_id;
    g_stub.last_read_attribute_id = attribute_id;
    g_stub.last_read_attribute_length = attribute_length;
    if (primitive_id == CSAP_ATTRIBUTE_READ_REQUEST && attribute_id == C_NODE_ADDRESS_ID)
    {
        g_stub.node_address_read_calls++;
        if (g_stub.node_address_read_result == 0)
        {
            uint32_encode_le(g_stub.node_address, attribute_value_p);
        }
        return g_stub.node_address_read_result;
    }

    if (primitive_id == CSAP_ATTRIBUTE_READ_REQUEST && attribute_id == C_NODE_ROLE_ID)
    {
        g_stub.role_read_calls++;
        if (g_stub.role_read_result == 0)
        {
            *attribute_value_p = g_stub.role;
        }
        return g_stub.role_read_result;
    }

    if (primitive_id == MSAP_ATTRIBUTE_READ_REQUEST && attribute_id == 1)
    {
        if (g_stub.msap_stack_status_read_result == 0)
        {
            *attribute_value_p = g_stub.stack_status;
        }
        return g_stub.msap_stack_status_read_result;
    }

    if (g_stub.attribute_read_result == 0)
    {
        std::memcpy(attribute_value_p, g_stub.attribute_read_value.data(), attribute_length);
    }
    return g_stub.attribute_read_result;
}

extern "C" int attribute_write_request(uint8_t primitive_id,
                                       uint16_t attribute_id,
                                       uint8_t attribute_length,
                                       const uint8_t * attribute_value_p)
{
    g_stub.last_write_primitive = primitive_id;
    g_stub.last_write_attribute_id = attribute_id;
    g_stub.last_write_attribute_length = attribute_length;
    std::memcpy(g_stub.last_write_value.data(), attribute_value_p, attribute_length);

    if (attribute_id == C_NODE_ADDRESS_ID)
    {
        if (g_stub.node_address_write_result == 0)
        {
            g_stub.node_address = uint32_decode_le(attribute_value_p);
        }
        return g_stub.node_address_write_result;
    }

    if (attribute_id == C_NODE_ROLE_ID)
    {
        if (g_stub.role_write_result == 0)
        {
            g_stub.role = attribute_value_p[0];
        }
        return g_stub.role_write_result;
    }

    return g_stub.attribute_write_result;
}

extern "C" int msap_stack_stop_request(void)
{
    return g_stub.msap_stack_stop_result;
}

extern "C" void WPC_Int_disable_poll_request(bool disable)
{
    g_stub.poll_request_states.push_back(disable);
}

extern "C" unsigned long long Platform_get_timestamp_ms_monotonic(void)
{
    return g_stub.monotonic_ms++;
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

class WpcSsrUnitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WPC_close();
        g_stub.reset();
    }
};

TEST_F(WpcSsrUnitTest, InitializePrimesSsrStateFromPreconfiguredSink)
{
    g_stub.role = APP_ROLE_SINK;
    g_stub.node_address = 0x11223344u;

    EXPECT_EQ(WPC_initialize("loopback", 125000), APP_RES_OK);
    EXPECT_EQ(g_stub.int_initialize_calls, 1);
    EXPECT_EQ(g_stub.int_set_mtu_calls, 1);
    EXPECT_EQ(g_stub.ssr_init_calls, 1);
    ASSERT_EQ(g_stub.sink_addresses.size(), 1u);
    EXPECT_EQ(g_stub.sink_addresses.front(), 0x11223344u);
    EXPECT_EQ(g_stub.ssr_clear_sink_address_calls, 2);
    EXPECT_EQ(g_stub.role_read_calls, 1);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
}

TEST_F(WpcSsrUnitTest, InitializeSkipsNodeAddressReadWhenNodeIsNotSink)
{
    g_stub.role = APP_ROLE_HEADNODE;
    g_stub.node_address = 0x11223344u;

    EXPECT_EQ(WPC_initialize("loopback", 125000), APP_RES_OK);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
    EXPECT_EQ(g_stub.role_read_calls, 1);
    EXPECT_EQ(g_stub.node_address_read_calls, 0);
    EXPECT_EQ(g_stub.ssr_clear_sink_address_calls, 2);
}

TEST_F(WpcSsrUnitTest, InitializeLeavesSsrDisabledWhenNodeAddressReadFails)
{
    g_stub.role = APP_ROLE_SINK;
    g_stub.node_address_read_result = 1;

    EXPECT_EQ(WPC_initialize("loopback", 125000), APP_RES_OK);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
    EXPECT_EQ(g_stub.role_read_calls, 1);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
    EXPECT_EQ(g_stub.ssr_clear_sink_address_calls, 2);
}

TEST_F(WpcSsrUnitTest, InitializeFailureSkipsSsrSetup)
{
    g_stub.int_initialize_result = -1;

    EXPECT_EQ(WPC_initialize("loopback", 125000), APP_RES_INTERNAL_ERROR);
    EXPECT_EQ(g_stub.int_set_mtu_calls, 0);
    EXPECT_EQ(g_stub.ssr_init_calls, 0);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
}

TEST_F(WpcSsrUnitTest, CloseDeinitializesSsrBeforeClosingTransport)
{
    WPC_close();

    ASSERT_EQ(g_stub.events.size(), 2u);
    EXPECT_EQ(g_stub.events[0], Event::SsrDeinit);
    EXPECT_EQ(g_stub.events[1], Event::IntClose);
    EXPECT_EQ(g_stub.ssr_deinit_calls, 1);
    EXPECT_EQ(g_stub.int_close_calls, 1);
}

TEST_F(WpcSsrUnitTest, StopStackResetsSsrRoutesWhenStopTriggersReboot)
{
    g_stub.msap_stack_stop_result = 0;
    g_stub.stack_status = 0;

    EXPECT_EQ(WPC_stop_stack(), APP_RES_OK);
    EXPECT_EQ(g_stub.ssr_reset_routes_calls, 1);
    ASSERT_EQ(g_stub.poll_request_states.size(), 2u);
    EXPECT_TRUE(g_stub.poll_request_states[0]);
    EXPECT_FALSE(g_stub.poll_request_states[1]);
}

TEST_F(WpcSsrUnitTest, SetNodeAddressPropagatesSuccessfulWriteToSsr)
{
    g_stub.node_address = 0x01020304u;
    ASSERT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);

    EXPECT_EQ(WPC_set_node_address(0x11223344u), APP_RES_OK);

    ASSERT_EQ(g_stub.sink_addresses.size(), 2u);
    EXPECT_EQ(g_stub.sink_addresses.back(), 0x11223344u);
    EXPECT_GE(g_stub.ssr_clear_sink_address_calls, 1);
    EXPECT_EQ(g_stub.last_write_primitive, CSAP_ATTRIBUTE_WRITE_REQUEST);
    EXPECT_EQ(g_stub.last_write_attribute_id, C_NODE_ADDRESS_ID);
    EXPECT_EQ(g_stub.last_write_attribute_length, 4u);
    EXPECT_EQ(uint32_decode_le(g_stub.last_write_value.data()), 0x11223344u);
}

TEST_F(WpcSsrUnitTest, SetNodeAddressDoesNotUpdateSsrOnWriteFailure)
{
    g_stub.node_address_write_result = 1;

    EXPECT_NE(WPC_set_node_address(0x11223344u), APP_RES_OK);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
}

TEST_F(WpcSsrUnitTest, SetNodeAddressClearsSsrWhenRoleIsNotSink)
{
    ASSERT_EQ(WPC_set_role(APP_ROLE_HEADNODE), APP_RES_OK);

    EXPECT_EQ(WPC_set_node_address(0x11223344u), APP_RES_OK);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
    EXPECT_GE(g_stub.ssr_clear_sink_address_calls, 1);
}

TEST_F(WpcSsrUnitTest, SetRoleConfiguresSsrWhenNodeBecomesSink)
{
    g_stub.node_address = 0x55667788u;

    EXPECT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);
    ASSERT_EQ(g_stub.sink_addresses.size(), 1u);
    EXPECT_EQ(g_stub.sink_addresses.front(), 0x55667788u);
    EXPECT_EQ(g_stub.role_read_calls, 0);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
}

TEST_F(WpcSsrUnitTest, GetRoleConfiguresSsrWhenNodeAddressWasKnownFirst)
{
    ASSERT_EQ(WPC_set_node_address(0x55667788u), APP_RES_OK);
    g_stub.node_address = 0x55667788u;
    g_stub.role = APP_ROLE_SINK;

    app_role_t role = APP_ROLE_UNKNOWN;
    EXPECT_EQ(WPC_get_role(&role), APP_RES_OK);
    EXPECT_EQ(role, APP_ROLE_SINK);
    ASSERT_EQ(g_stub.sink_addresses.size(), 2u);
    EXPECT_EQ(g_stub.sink_addresses.back(), 0x55667788u);
    EXPECT_EQ(g_stub.role_read_calls, 1);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
}

TEST_F(WpcSsrUnitTest, GetRolePrimesSsrNodeAddressWhenNodeIsSink)
{
    g_stub.role = APP_ROLE_SINK;
    g_stub.node_address = 0x10203040u;

    app_role_t role = APP_ROLE_UNKNOWN;
    EXPECT_EQ(WPC_get_role(&role), APP_RES_OK);
    EXPECT_EQ(role, APP_ROLE_SINK);
    ASSERT_EQ(g_stub.sink_addresses.size(), 1u);
    EXPECT_EQ(g_stub.sink_addresses.front(), 0x10203040u);
    EXPECT_EQ(g_stub.role_read_calls, 1);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
}

TEST_F(WpcSsrUnitTest, SetRoleClearsSsrWhenNodeStopsBeingSink)
{
    EXPECT_EQ(WPC_set_role(APP_ROLE_HEADNODE), APP_RES_OK);
    EXPECT_TRUE(g_stub.sink_addresses.empty());
    EXPECT_EQ(g_stub.ssr_clear_sink_address_calls, 1);
}

TEST_F(WpcSsrUnitTest, SendDataUsesSsrRequestWhenRouteExistsOnSink)
{
    static const uint8_t bytes[] = { 0x10, 0x20, 0x30 };
    ASSERT_EQ(WPC_set_node_address(0x01020304u), APP_RES_OK);
    ASSERT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);
    g_stub.ssr_has_route = true;
    g_stub.ssr_first_hop = 0x0A0B0C0Du;

    app_message_t message = {};
    message.bytes = bytes;
    message.num_bytes = sizeof(bytes);
    message.pdu_id = 0x1234u;
    message.dst_addr = 0x11223344u;
    message.qos = APP_QOS_HIGH;
    message.src_ep = 5u;
    message.dst_ep = 6u;
    message.buffering_delay = 1500u;
    message.hop_limit = 7u;
    message.is_unack_csma_ca = true;

    EXPECT_EQ(WPC_send_data_with_options(&message), APP_RES_OK);
    EXPECT_EQ(g_stub.ssr_lookup_calls, 1);
    EXPECT_EQ(g_stub.last_ssr_lookup_dest, 0x11223344u);
    EXPECT_TRUE(g_stub.dsap_ssr_called);
    EXPECT_FALSE(g_stub.dsap_called);

    EXPECT_EQ(g_stub.last_dsap_ssr_call.bytes, std::vector<uint8_t>(bytes, bytes + sizeof(bytes)));
    EXPECT_EQ(g_stub.last_dsap_ssr_call.pdu_id, 0x1234u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.dest_add, 0x11223344u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.qos, 1u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.src_ep, 5u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.dest_ep, 6u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.buffering_delay, ms_to_internal_time(1500u));
    EXPECT_TRUE(g_stub.last_dsap_ssr_call.is_unack_csma_ca);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.hop_limit, 7u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.hop_count, 1u);
    ASSERT_EQ(g_stub.last_dsap_ssr_call.hops.size(), 1u);
    EXPECT_EQ(g_stub.last_dsap_ssr_call.hops[0], 0x0A0B0C0Du);
    EXPECT_EQ(g_stub.role_read_calls, 0);
}

TEST_F(WpcSsrUnitTest, SendDataFallsBackToRegularRequestWhenRouteIsMissing)
{
    static const uint8_t bytes[] = { 0xAA, 0xBB };
    ASSERT_EQ(WPC_set_node_address(0x01020304u), APP_RES_OK);
    ASSERT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);

    app_message_t message = {};
    message.bytes = bytes;
    message.num_bytes = sizeof(bytes);
    message.pdu_id = 0x4321u;
    message.dst_addr = 0x55667788u;
    message.qos = APP_QOS_NORMAL;
    message.src_ep = 3u;
    message.dst_ep = 4u;
    message.buffering_delay = 500u;
    message.hop_limit = 2u;

    EXPECT_EQ(WPC_send_data_with_options(&message), APP_RES_OK);
    EXPECT_TRUE(g_stub.dsap_called);
    EXPECT_FALSE(g_stub.dsap_ssr_called);
    EXPECT_EQ(g_stub.last_dsap_call.dest_add, 0x55667788u);
    EXPECT_EQ(g_stub.last_dsap_call.pdu_id, 0x4321u);
    EXPECT_EQ(g_stub.last_dsap_call.qos, 0u);
    EXPECT_EQ(g_stub.last_dsap_call.buffering_delay, ms_to_internal_time(500u));
    EXPECT_EQ(g_stub.last_dsap_call.hop_limit, 2u);
    EXPECT_EQ(g_stub.role_read_calls, 0);
}

TEST_F(WpcSsrUnitTest, SendDataFallsBackToRegularRequestWhenSinkNodeAddressReadFails)
{
    static const uint8_t bytes[] = { 0x11, 0x22 };
    g_stub.node_address_read_result = 1;
    ASSERT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);
    g_stub.ssr_has_route = true;
    g_stub.ssr_first_hop = 0xABCDEF01u;

    app_message_t message = {};
    message.bytes = bytes;
    message.num_bytes = sizeof(bytes);
    message.pdu_id = 0x2222u;
    message.dst_addr = 0x55667788u;

    EXPECT_EQ(WPC_send_data_with_options(&message), APP_RES_OK);
    EXPECT_TRUE(g_stub.dsap_called);
    EXPECT_FALSE(g_stub.dsap_ssr_called);
    EXPECT_EQ(g_stub.ssr_lookup_calls, 0);
    EXPECT_EQ(g_stub.node_address_read_calls, 1);
}

TEST_F(WpcSsrUnitTest, SendDataFallsBackToRegularRequestWhenNodeIsNotSink)
{
    static const uint8_t bytes[] = { 0xCC, 0xDD };
    ASSERT_EQ(WPC_set_role(APP_ROLE_HEADNODE), APP_RES_OK);
    g_stub.ssr_has_route = true;
    g_stub.ssr_first_hop = 0x11111111u;

    app_message_t message = {};
    message.bytes = bytes;
    message.num_bytes = sizeof(bytes);
    message.pdu_id = 0x4321u;
    message.dst_addr = 0x55667788u;

    EXPECT_EQ(WPC_send_data_with_options(&message), APP_RES_OK);
    EXPECT_TRUE(g_stub.dsap_called);
    EXPECT_FALSE(g_stub.dsap_ssr_called);
    EXPECT_EQ(g_stub.ssr_lookup_calls, 0);
}

TEST_F(WpcSsrUnitTest, SendDataMapsSsrTransportErrorsThroughExistingLut)
{
    static const uint8_t bytes[] = { 0x01 };
    ASSERT_EQ(WPC_set_node_address(0x01020304u), APP_RES_OK);
    ASSERT_EQ(WPC_set_role(APP_ROLE_SINK), APP_RES_OK);
    g_stub.ssr_has_route = true;
    g_stub.ssr_first_hop = 0x22222222u;
    g_stub.dsap_ssr_result = 5;

    app_message_t message = {};
    message.bytes = bytes;
    message.num_bytes = sizeof(bytes);
    message.pdu_id = 1u;
    message.dst_addr = 2u;

    EXPECT_EQ(WPC_send_data_with_options(&message), APP_RES_UNKNOWN_DEST);
}
