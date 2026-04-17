/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 * Unit tests for the SSR bookkeeping module (FHT + SSR integration layer).
 *
 * These tests do NOT require a connected Wirepas sink; they exercise only the
 * in-process data structures.  Platform_malloc/free map to malloc/free on
 * Linux, so no WPC_initialize() call is needed.
 */

#include <gtest/gtest.h>

extern "C"
{
#include "fht.h"
#include "ssr.h"
#include "dsap.h"
#include "msap.h"
}

#include <cstdint>
#include <cstring>
#include <climits>

/* -------------------------------------------------------------------------- */
/* FHT lifecycle                                                               */
/* -------------------------------------------------------------------------- */

TEST(FhtLifecycle, CreateDestroy)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_destroy(t);
}

TEST(FhtLifecycle, DestroyNull)
{
    /* Must not crash. */
    EXPECT_NO_FATAL_FAILURE(fht_destroy(nullptr));
}

TEST(FhtLifecycle, DefaultValidity)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(fht_get_validity(t), 2400u);
    fht_destroy(t);
}

TEST(FhtLifecycle, SetGetValidity)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, 600u);
    EXPECT_EQ(fht_get_validity(t), 600u);
    fht_destroy(t);
}

/* -------------------------------------------------------------------------- */
/* FHT insert and lookup                                                       */
/* -------------------------------------------------------------------------- */

class FhtInsertLookup : public ::testing::Test
{
protected:
    void SetUp() override
    {
        t = fht_create();
        ASSERT_NE(t, nullptr);
        /* Use a large validity so entries never expire during tests. */
        fht_set_validity(t, UINT32_MAX / 2);
    }

    void TearDown() override { fht_destroy(t); }

    fht_t * t = nullptr;
};

TEST_F(FhtInsertLookup, InsertAndLookup)
{
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAAAA, 100), 0);

    uint32_t hop = 0;
    fht_get_first_hop(t, 0x1000, 101, &hop);
    EXPECT_EQ(hop, 0xAAAAu);
}

TEST_F(FhtInsertLookup, LookupMissingReturnsZero)
{
    uint32_t hop = 0xDEAD;
    fht_get_first_hop(t, 0x9999, 100, &hop);
    EXPECT_EQ(hop, 0u);
}

TEST_F(FhtInsertLookup, UpdateOverwritesOlderTimestamp)
{
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAAAA, 100), 0);
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xCCCC, 200), 0);

    uint32_t hop = 0;
    fht_get_first_hop(t, 0x1000, 201, &hop);
    EXPECT_EQ(hop, 0xCCCCu);
}

TEST_F(FhtInsertLookup, MultipleDifferentNodes)
{
    ASSERT_EQ(fht_update_route(t, 0x0001, 0xAA01, 10), 0);
    ASSERT_EQ(fht_update_route(t, 0x0002, 0xAA02, 10), 0);
    ASSERT_EQ(fht_update_route(t, 0x0003, 0xAA03, 10), 0);

    uint32_t hop;
    fht_get_first_hop(t, 0x0001, 11, &hop);
    EXPECT_EQ(hop, 0xAA01u);

    fht_get_first_hop(t, 0x0002, 11, &hop);
    EXPECT_EQ(hop, 0xAA02u);

    fht_get_first_hop(t, 0x0003, 11, &hop);
    EXPECT_EQ(hop, 0xAA03u);
}

/* -------------------------------------------------------------------------- */
/* FHT validity / expiry                                                       */
/* -------------------------------------------------------------------------- */

class FhtValidity : public ::testing::Test
{
protected:
    void SetUp() override
    {
        t = fht_create();
        ASSERT_NE(t, nullptr);
        fht_set_validity(t, 100); /* 100 second window */
    }

    void TearDown() override { fht_destroy(t); }

    fht_t * t = nullptr;
};

TEST_F(FhtValidity, EntryValidBeforeExpiry)
{
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAAAA, 1000), 0);

    uint32_t hop = 0;
    /* now = 1099; delta = 99 < validity 100 → still valid */
    fht_get_first_hop(t, 0x1000, 1099, &hop);
    EXPECT_EQ(hop, 0xAAAAu);
}

TEST_F(FhtValidity, EntryExpiredAfterWindow)
{
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAAAA, 1000), 0);

    uint32_t hop = 0xDEAD;
    /* now = 1100; delta = 100 >= validity 100 → expired */
    fht_get_first_hop(t, 0x1000, 1100, &hop);
    EXPECT_EQ(hop, 0u);
}

/* -------------------------------------------------------------------------- */
/* FHT "no overwrite with stale update"                                        */
/* -------------------------------------------------------------------------- */

TEST(FhtNoOverwrite, OlderTimestampIgnored)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, UINT32_MAX / 2);

    /* Insert with ts=200 first. */
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAA01, 200), 0);
    /* This update is older (ts=100 < stored 200) and must be silently ignored. */
    ASSERT_EQ(fht_update_route(t, 0x1000, 0xAA02, 100), 0);

    uint32_t hop = 0;
    fht_get_first_hop(t, 0x1000, 201, &hop);
    /* First (newer) update must win. */
    EXPECT_EQ(hop, 0xAA01u);

    fht_destroy(t);
}

/* -------------------------------------------------------------------------- */
/* FHT purge                                                                   */
/* -------------------------------------------------------------------------- */

TEST(FhtPurge, ExpiredEntriesRemovedOnPurge)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, 50);

    /* Insert one fresh and one soon-to-expire entry. */
    ASSERT_EQ(fht_update_route(t, 0x0001, 0xA001, 1000), 0);
    ASSERT_EQ(fht_update_route(t, 0x0002, 0xA002, 900), 0);

    /* At t=1060: node 0x0002 has age 160 > 50 → expired; 0x0001 has age 60 > 50 → expired too. */
    fht_purge_expired(t, 1060);

    uint32_t hop;
    fht_get_first_hop(t, 0x0001, 1060, &hop);
    EXPECT_EQ(hop, 0u); /* expired at purge */
    fht_get_first_hop(t, 0x0002, 1060, &hop);
    EXPECT_EQ(hop, 0u); /* expired at purge */

    fht_destroy(t);
}

TEST(FhtPurge, FreshEntryKeptAfterPurge)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, 50);

    ASSERT_EQ(fht_update_route(t, 0x0001, 0xA001, 1000), 0);
    ASSERT_EQ(fht_update_route(t, 0x0002, 0xA002, 900), 0);

    /* At t=1040: 0x0001 has age 40 < 50 (fresh); 0x0002 has age 140 > 50 (stale). */
    fht_purge_expired(t, 1040);

    uint32_t hop;
    fht_get_first_hop(t, 0x0001, 1040, &hop);
    EXPECT_EQ(hop, 0xA001u); /* still valid */

    fht_get_first_hop(t, 0x0002, 1040, &hop);
    EXPECT_EQ(hop, 0u); /* purged */

    fht_destroy(t);
}

/* -------------------------------------------------------------------------- */
/* FHT resize (load-factor triggered growth)                                   */
/* -------------------------------------------------------------------------- */

TEST(FhtResize, GrowsBeyondInitialCapacity)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, UINT32_MAX / 2);

    /* Insert enough entries to exceed the 0.7 load factor of the initial
     * 64-slot table (>44 entries).  The table should resize transparently. */
    const int N = 100;
    for (int i = 1; i <= N; i++)
    {
        ASSERT_EQ(fht_update_route(t, (uint32_t) i, (uint32_t) (i + 1000), 500), 0)
            << "insert failed at i=" << i;
    }

    /* Verify all entries are still reachable. */
    for (int i = 1; i <= N; i++)
    {
        uint32_t hop = 0;
        fht_get_first_hop(t, (uint32_t) i, 501, &hop);
        EXPECT_EQ(hop, (uint32_t) (i + 1000)) << "wrong hop for node " << i;
    }

    fht_destroy(t);
}

/* -------------------------------------------------------------------------- */
/* FHT edge cases                                                              */
/* -------------------------------------------------------------------------- */

TEST(FhtEdgeCases, NodeIdZero)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, UINT32_MAX / 2);

    ASSERT_EQ(fht_update_route(t, 0u, 0xAAAA, 10), 0);

    uint32_t hop = 0;
    fht_get_first_hop(t, 0u, 11, &hop);
    EXPECT_EQ(hop, 0xAAAAu);

    fht_destroy(t);
}

TEST(FhtEdgeCases, NodeIdMax)
{
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, UINT32_MAX / 2);

    ASSERT_EQ(fht_update_route(t, UINT32_MAX, 0x1234, 10), 0);

    uint32_t hop = 0;
    fht_get_first_hop(t, UINT32_MAX, 11, &hop);
    EXPECT_EQ(hop, 0x1234u);

    fht_destroy(t);
}

TEST(FhtEdgeCases, ManyCollisionsProbedCorrectly)
{
    /* Insert a batch of consecutive IDs that are likely to share hash buckets,
     * then verify every entry is retrievable. */
    fht_t * t = fht_create();
    ASSERT_NE(t, nullptr);
    fht_set_validity(t, UINT32_MAX / 2);

    const int N = 60; /* slightly less than initial capacity to stay in one table */
    for (int i = 0; i < N; i++)
    {
        ASSERT_EQ(fht_update_route(t, (uint32_t) i, (uint32_t) (i + 500), 1), 0);
    }
    for (int i = 0; i < N; i++)
    {
        uint32_t hop = 0;
        fht_get_first_hop(t, (uint32_t) i, 2, &hop);
        EXPECT_EQ(hop, (uint32_t) (i + 500)) << "collision probe failed for node " << i;
    }

    fht_destroy(t);
}

/* -------------------------------------------------------------------------- */
/* SSR module                                                                  */
/* -------------------------------------------------------------------------- */

class SsrModuleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ssr_init();
    }

    void TearDown() override
    {
        ssr_deinit();
    }
};

TEST_F(SsrModuleTest, InitIdempotent)
{
    /* Second init must not crash or leak. */
    EXPECT_NO_FATAL_FAILURE(ssr_init());
}

TEST_F(SsrModuleTest, LookupAfterRegistration)
{
    /* Simulate a registration arriving with zero delay. */
    ssr_set_sink_address(0xBBBB);
    ssr_on_registration(0x1111, 0xAAAA, 0xBBBB, 0);

    uint32_t hop = 0;
    bool found = ssr_get_first_hop(0x1111, &hop);
    EXPECT_TRUE(found);
    EXPECT_EQ(hop, 0xAAAAu);
}

TEST_F(SsrModuleTest, LookupMissingNodeReturnsFalse)
{
    uint32_t hop = 0xDEAD;
    bool found = ssr_get_first_hop(0xDEAD, &hop);
    EXPECT_FALSE(found);
    EXPECT_EQ(hop, 0u);
}

/* -------------------------------------------------------------------------- */
/* SSR sink filtering                                                          */
/* -------------------------------------------------------------------------- */

TEST_F(SsrModuleTest, RegistrationFromUnexpectedSinkIsDiscarded)
{
    ssr_set_sink_address(0xBEEF0001u);
    ssr_on_registration(0x5000, 0xAA01, 0xBEEF0002u, 0);

    uint32_t hop = 0;
    bool found = ssr_get_first_hop(0x5000, &hop);
    EXPECT_FALSE(found);
    EXPECT_EQ(hop, 0u);
}

/* -------------------------------------------------------------------------- */
/* SSR callback                                                                */
/* -------------------------------------------------------------------------- */

static uint32_t g_cb_src   = 0;
static uint32_t g_cb_hop   = 0;
static uint32_t g_cb_delay = 0;
static int      g_cb_count = 0;

static void ssr_test_cb(uint32_t src, uint32_t hop, uint32_t delay)
{
    g_cb_src   = src;
    g_cb_hop   = hop;
    g_cb_delay = delay;
    g_cb_count++;
}

TEST_F(SsrModuleTest, CallbackInvokedOnRegistration)
{
    g_cb_count = 0;

    ASSERT_TRUE(ssr_register_for_registration(ssr_test_cb));

    ssr_set_sink_address(0xDDDD);
    ssr_on_registration(0x2222, 0xCCCC, 0xDDDD, 512);

    EXPECT_EQ(g_cb_count, 1);
    EXPECT_EQ(g_cb_src,   0x2222u);
    EXPECT_EQ(g_cb_hop,   0xCCCCu);
    EXPECT_EQ(g_cb_delay, 512u);

    ASSERT_TRUE(ssr_unregister_from_registration());
}

TEST_F(SsrModuleTest, RegisterTwiceFails)
{
    ASSERT_TRUE(ssr_register_for_registration(ssr_test_cb));
    EXPECT_FALSE(ssr_register_for_registration(ssr_test_cb));
    ASSERT_TRUE(ssr_unregister_from_registration());
}

TEST_F(SsrModuleTest, UnregisterWhenNotRegisteredReturnsFalse)
{
    EXPECT_FALSE(ssr_unregister_from_registration());
}

/* -------------------------------------------------------------------------- */
/* Frame layout assertions                                                     */
/* -------------------------------------------------------------------------- */

TEST(FrameLayout, SsrTxFrameFitsInPayloadByte)
{
    /* The wpc_frame_t union payload must fit in UINT8_MAX bytes. */
    EXPECT_LE(sizeof(dsap_data_tx_ssr_req_pl_t), 255u);
}

TEST(FrameLayout, SsrTxFragFrameFitsInPayloadByte)
{
    EXPECT_LE(sizeof(dsap_data_tx_ssr_frag_req_pl_t), 255u);
}

TEST(FrameLayout, MsapSsrRegFitsInPayloadByte)
{
    EXPECT_LE(sizeof(msap_ssr_registration_ind_pl_t), 255u);
}

TEST(FrameLayout, SsrTxHopCountFieldOffset)
{
    /* Validate that hop_count is located after the fixed header fields
     * (pdu_id, src_ep, dest_add, dest_ep, qos, tx_options, buffering_delay). */
    constexpr size_t expected_offset =
        sizeof(uint16_t)   /* pdu_id         */
        + sizeof(uint8_t)  /* src_endpoint   */
        + sizeof(uint32_t) /* dest_add       */
        + sizeof(uint8_t)  /* dest_endpoint  */
        + sizeof(uint8_t)  /* qos            */
        + sizeof(uint8_t)  /* tx_options     */
        + sizeof(uint32_t) /* buffering_delay */;

    EXPECT_EQ(offsetof(dsap_data_tx_ssr_req_pl_t, hop_count), expected_offset);
}

TEST(FrameLayout, SsrTxFragHopCountFieldOffset)
{
    constexpr size_t expected_offset =
        sizeof(uint16_t)   /* pdu_id               */
        + sizeof(uint8_t)  /* src_endpoint         */
        + sizeof(uint32_t) /* dest_add             */
        + sizeof(uint8_t)  /* dest_endpoint        */
        + sizeof(uint8_t)  /* qos                  */
        + sizeof(uint8_t)  /* tx_options           */
        + sizeof(uint32_t) /* buffering_delay      */
        + sizeof(uint16_t) /* full_packet_id bits  */
        + sizeof(uint16_t) /* fragment_offset_flag */;

    EXPECT_EQ(offsetof(dsap_data_tx_ssr_frag_req_pl_t, hop_count), expected_offset);
}

TEST(FrameLayout, MsapSsrRegFieldOrder)
{
    /* indication_status must be first. */
    EXPECT_EQ(offsetof(msap_ssr_registration_ind_pl_t, indication_status), 0u);

    /* source_address immediately after. */
    EXPECT_EQ(offsetof(msap_ssr_registration_ind_pl_t, source_address),
              sizeof(uint8_t));
}

TEST(FrameLayout, SsrMaxHopsConstant)
{
    EXPECT_EQ(SSR_MAX_HOPS, 3);
}
