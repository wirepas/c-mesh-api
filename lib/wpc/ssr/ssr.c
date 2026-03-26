/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 * SSR integration layer for c-mesh-api.
 *
 * The module stores the first hop learned from SSR registrations and exposes
 * sink-filtered lookups for the send path.
 */
#define LOG_MODULE_NAME "ssr"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "ssr.h"       /* lib/wpc/include/ssr.h via PRIVATE include path */
#include "ssr_backend.h"
#include "fht.h"       /* lib/wpc/include/fht.h via PRIVATE include path */
#include "platform.h"

static fht_t *                m_fht                     = NULL;
static onSsrRegistration_cb_f m_reg_cb                  = NULL;
static uint32_t               m_sink_address            = 0;
static bool                   m_sink_address_configured = false;

static void clear_route_table_locked(void)
{
    if (m_fht != NULL)
    {
        fht_clear(m_fht);
    }
}

void ssr_set_sink_address_locked(uint32_t sink_address)
{
    if (!m_sink_address_configured || m_sink_address != sink_address)
    {
        clear_route_table_locked();
    }

    m_sink_address = sink_address;
    m_sink_address_configured = true;
}

void ssr_clear_sink_address_locked(void)
{
    clear_route_table_locked();
    m_sink_address = 0;
    m_sink_address_configured = false;
}

void ssr_reset_routes_locked(void)
{
    clear_route_table_locked();
}

bool ssr_get_first_hop_locked(uint32_t  dest,
                              uint32_t *first_hop_id)
{
    uint32_t now = (uint32_t) (Platform_get_timestamp_ms_monotonic() / 1000);
    uint32_t hop = 0;

    *first_hop_id = 0;

    if (m_fht == NULL)
    {
        return false;
    }

    fht_get_first_hop(m_fht, dest, now, &hop);
    if (hop == 0)
    {
        return false;
    }

    *first_hop_id = hop;
    return true;
}

void ssr_init(void)
{
    if (!Platform_lock_ssr())
        return;

    if (m_fht != NULL)
    {
        /* Already initialised; idempotent. */
        Platform_unlock_ssr();
        return;
    }

    m_fht = fht_create();
    if (m_fht == NULL)
    {
        LOGE("SSR: failed to allocate First-Hop Table\n");
    }
    else
    {
        LOGI("SSR: initialised\n");
    }

    Platform_unlock_ssr();
}

void ssr_deinit(void)
{
    if (!Platform_lock_ssr())
        return;

    fht_destroy(m_fht);
    m_fht                     = NULL;
    m_reg_cb                  = NULL;
    m_sink_address            = 0;
    m_sink_address_configured = false;

    Platform_unlock_ssr();
}

void ssr_set_sink_address(uint32_t sink_address)
{
    if (!Platform_lock_ssr())
        return;

    ssr_set_sink_address_locked(sink_address);
    Platform_unlock_ssr();
}

void ssr_clear_sink_address(void)
{
    if (!Platform_lock_ssr())
        return;

    ssr_clear_sink_address_locked();
    Platform_unlock_ssr();
}

void ssr_reset_routes(void)
{
    if (!Platform_lock_ssr())
        return;

    ssr_reset_routes_locked();
    Platform_unlock_ssr();
}

void ssr_on_registration(uint32_t source_address,
                         uint32_t source_routing_id,
                         uint32_t sink_address,
                         uint32_t delay_hp)
{
    onSsrRegistration_cb_f reg_cb = NULL;

    if (!Platform_lock_ssr())
        return;

    if (m_fht == NULL)
    {
        Platform_unlock_ssr();
        return;
    }

    if (!m_sink_address_configured)
    {
        LOGW("SSR registration discarded: local sink address not configured\n");
        Platform_unlock_ssr();
        return;
    }

    if (sink_address != m_sink_address)
    {
        LOGW("SSR registration discarded: src=%u hop=%u sink=%u expected=%u\n",
             source_address, source_routing_id, sink_address, m_sink_address);
        Platform_unlock_ssr();
        return;
    }

    uint32_t now = (uint32_t) (Platform_get_timestamp_ms_monotonic() / 1000);
    uint32_t delay_s = delay_hp / 1024;
    /* Back-calculate generation time: delay_hp is in 1/1024 s units. */
    uint32_t generated_at = delay_s >= now ? 0 : now - delay_s;

    LOGI("SSR registration: src=%u hop=%u sink=%u delay_hp=%u ts=%u\n",
         source_address, source_routing_id, sink_address, delay_hp, generated_at);

    fht_update_route(m_fht,
                     source_address,
                     source_routing_id,
                     generated_at);

    reg_cb = m_reg_cb;
    Platform_unlock_ssr();

    if (reg_cb != NULL)
    {
        reg_cb(source_address, source_routing_id, delay_hp);
    }
}

bool ssr_get_first_hop(uint32_t  dest,
                       uint32_t *first_hop_id)
{
    if (!Platform_lock_ssr())
        return false;
    bool found = ssr_get_first_hop_locked(dest, first_hop_id);
    Platform_unlock_ssr();
    return found;
}

void ssr_purge_expired(void)
{
    if (!Platform_lock_ssr())
        return;

    if (m_fht == NULL)
    {
        Platform_unlock_ssr();
        return;
    }

    uint32_t now = (uint32_t) (Platform_get_timestamp_ms_monotonic() / 1000);
    fht_purge_expired(m_fht, now);
    Platform_unlock_ssr();
}

bool ssr_register_for_registration(onSsrRegistration_cb_f cb)
{
    if (!Platform_lock_ssr())
        return false;

    if (m_reg_cb != NULL)
    {
        Platform_unlock_ssr();
        return false;
    }
    m_reg_cb = cb;
    Platform_unlock_ssr();
    return true;
}

bool ssr_unregister_from_registration(void)
{
    if (!Platform_lock_ssr())
        return false;

    if (m_reg_cb == NULL)
    {
        Platform_unlock_ssr();
        return false;
    }
    m_reg_cb = NULL;
    Platform_unlock_ssr();
    return true;
}
