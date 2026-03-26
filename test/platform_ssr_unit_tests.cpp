#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#define _Static_assert static_assert
extern "C"
{
#include "platform.h"
}

namespace
{
std::atomic<int> g_gc_calls{0};
std::atomic<int> g_ssr_purge_calls{0};

int no_indication(unsigned int max_ind, onIndicationReceivedLocked_cb_f cb_locked)
{
    (void) max_ind;
    (void) cb_locked;
    return 0;
}

void ignore_indication(wpc_frame_t * frame, unsigned long long timestamp_ms)
{
    (void) frame;
    (void) timestamp_ms;
}

bool wait_for(std::atomic<int> & counter, int expected, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (counter.load() >= expected)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return counter.load() >= expected;
}
} // namespace

extern "C" bool reassembly_is_queue_empty(void)
{
    return true;
}

extern "C" void reassembly_garbage_collect(void)
{
    g_gc_calls.fetch_add(1);
}

extern "C" void ssr_purge_expired(void)
{
    g_ssr_purge_calls.fetch_add(1);
}

TEST(PlatformSsrUnitTest, IdleDispatchWakeupPurgesExpiredSsrRoutes)
{
    g_gc_calls.store(0);
    g_ssr_purge_calls.store(0);

    ASSERT_TRUE(Platform_init(no_indication, ignore_indication));
    EXPECT_TRUE(wait_for(g_gc_calls, 1, std::chrono::milliseconds(6500)));
    EXPECT_TRUE(wait_for(g_ssr_purge_calls, 1, std::chrono::milliseconds(6500)));
    Platform_close();
}
