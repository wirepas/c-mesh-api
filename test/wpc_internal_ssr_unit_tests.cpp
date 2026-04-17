#include <gtest/gtest.h>

#include <cstdint>
#include <cstdarg>

#define _Static_assert static_assert
extern "C"
{
#include "dsap.h"
#include "msap.h"
#include "platform.h"
#include "slip.h"
#include "wpc_constants.h"
#include "wpc_internal.h"
}

namespace
{
Platform_dispatch_indication_f g_dispatch_indication = nullptr;
Platform_get_indication_f g_get_indication = nullptr;
bool g_platform_close_called = false;
int g_serial_close_calls = 0;
int g_dsap_init_calls = 0;

struct RegistrationSpy
{
    int calls = 0;
    msap_ssr_registration_ind_pl_t payload = {};

    void reset()
    {
        calls = 0;
        payload = {};
    }
} g_registration_spy;
} // namespace

extern "C" int Serial_open(const char * port_name, unsigned long bitrate)
{
    (void) port_name;
    (void) bitrate;
    return 0;
}

extern "C" int Serial_close(void)
{
    g_serial_close_calls++;
    return 0;
}

extern "C" int Serial_read(unsigned char * c, unsigned int timeout_ms)
{
    (void) c;
    (void) timeout_ms;
    return 0;
}

extern "C" int Serial_write(const unsigned char * buffer, unsigned int buffer_size)
{
    (void) buffer;
    (void) buffer_size;
    return 0;
}

extern "C" int Slip_init(write_f write_fn, read_f read_fn)
{
    (void) write_fn;
    (void) read_fn;
    return 0;
}

extern "C" int Slip_send_buffer(uint8_t * buffer, uint32_t len)
{
    (void) buffer;
    (void) len;
    return 0;
}

extern "C" int Slip_get_buffer(uint8_t * buffer, uint32_t len, uint16_t timeout_ms)
{
    (void) buffer;
    (void) len;
    (void) timeout_ms;
    return -1;
}

extern "C" bool Platform_init(Platform_get_indication_f get_indication_f,
                              Platform_dispatch_indication_f dispatch_indication_f)
{
    g_get_indication = get_indication_f;
    g_dispatch_indication = dispatch_indication_f;
    return true;
}

extern "C" void Platform_close(void)
{
    g_platform_close_called = true;
}

extern "C" unsigned long long Platform_get_timestamp_ms_monotonic(void)
{
    return 1234;
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

extern "C" unsigned long long Platform_get_timestamp_ms_epoch(void)
{
    return 0;
}

extern "C" bool Platform_lock_request(void)
{
    return true;
}

extern "C" void Platform_unlock_request(void)
{
}

extern "C" void dsap_init(void)
{
    g_dsap_init_calls++;
}

extern "C" void dsap_data_tx_indication_handler(dsap_data_tx_ind_pl_t * payload)
{
    (void) payload;
}

extern "C" void dsap_data_rx_indication_handler(dsap_data_rx_ind_pl_t * payload,
                                                unsigned long long timestamp_ms)
{
    (void) payload;
    (void) timestamp_ms;
}

extern "C" void dsap_data_rx_frag_indication_handler(dsap_data_rx_frag_ind_pl_t * payload,
                                                     unsigned long long timestamp_ms)
{
    (void) payload;
    (void) timestamp_ms;
}

extern "C" void msap_stack_state_indication_handler(msap_stack_state_ind_pl_t * payload)
{
    (void) payload;
}

extern "C" void msap_app_config_data_rx_indication_handler(msap_app_config_data_rx_ind_pl_t * payload)
{
    (void) payload;
}

extern "C" void msap_scan_nbors_indication_handler(msap_scan_nbors_ind_pl_t * payload)
{
    (void) payload;
}

extern "C" void msap_config_data_item_rx_indication_handler(msap_config_data_item_rx_ind_pl_t * payload)
{
    (void) payload;
}

extern "C" void msap_ssr_registration_indication_handler(msap_ssr_registration_ind_pl_t * payload)
{
    g_registration_spy.calls++;
    g_registration_spy.payload = *payload;
}

TEST(WpcInternalSsrUnitTest, DispatchCallbackRoutesSsrRegistrationIndications)
{
    g_dispatch_indication = nullptr;
    g_get_indication = nullptr;
    g_platform_close_called = false;
    g_serial_close_calls = 0;
    g_dsap_init_calls = 0;
    g_registration_spy.reset();

    ASSERT_EQ(WPC_Int_initialize("loopback", 125000), 0);
    ASSERT_NE(g_get_indication, nullptr);
    ASSERT_NE(g_dispatch_indication, nullptr);
    EXPECT_EQ(g_dsap_init_calls, 1);

    wpc_frame_t frame = {};
    frame.primitive_id = MSAP_SSR_REGISTRATION_INDICATION;
    frame.payload.msap_ssr_registration_indication_payload.source_address = 0x1001u;
    frame.payload.msap_ssr_registration_indication_payload.source_routing_id = 0x2002u;
    frame.payload.msap_ssr_registration_indication_payload.sink_address = 0x3003u;
    frame.payload.msap_ssr_registration_indication_payload.delay_hp = 0x4004u;

    g_dispatch_indication(&frame, 999u);

    ASSERT_EQ(g_registration_spy.calls, 1);
    EXPECT_EQ(g_registration_spy.payload.source_address, 0x1001u);
    EXPECT_EQ(g_registration_spy.payload.source_routing_id, 0x2002u);
    EXPECT_EQ(g_registration_spy.payload.sink_address, 0x3003u);
    EXPECT_EQ(g_registration_spy.payload.delay_hp, 0x4004u);

    WPC_Int_close();
    EXPECT_TRUE(g_platform_close_called);
    EXPECT_EQ(g_serial_close_calls, 1);
}
