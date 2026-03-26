/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "wpc"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "platform.h"
#include "ssr_backend.h"
#include "ssr.h"
#include "wpc_ssr.h"

static bool       m_ssr_role_known         = false;
static app_role_t m_ssr_role               = APP_ROLE_UNKNOWN;
static bool       m_ssr_node_address_known = false;
static app_addr_t m_ssr_node_address       = 0;

static bool is_sink_role(app_role_t role)
{
    return GET_BASE_ROLE(role) == APP_ROLE_SINK;
}

static void reset_cached_ssr_state(void)
{
    m_ssr_role_known = false;
    m_ssr_role = APP_ROLE_UNKNOWN;
    m_ssr_node_address_known = false;
    m_ssr_node_address = 0;
}

static void apply_cached_ssr_state_locked(void)
{
    if (!m_ssr_role_known || !is_sink_role(m_ssr_role) || !m_ssr_node_address_known)
    {
        ssr_clear_sink_address_locked();
        return;
    }

    ssr_set_sink_address_locked(m_ssr_node_address);
}

void wpc_ssr_reset(void)
{
    reset_cached_ssr_state();
}

void wpc_ssr_init(void)
{
    ssr_init();
    if (!Platform_lock_ssr())
    {
        return;
    }

    ssr_clear_sink_address_locked();
    Platform_unlock_ssr();
}

void wpc_ssr_close(void)
{
    ssr_deinit();
    wpc_ssr_reset();
}

void wpc_ssr_reset_routes(void)
{
    ssr_reset_routes();
}

static void cache_role(app_role_t role)
{
    m_ssr_role_known = true;
    m_ssr_role = role;
}

void wpc_ssr_on_role_read(app_role_t role)
{
    if (!Platform_lock_ssr())
    {
        return;
    }

    cache_role(role);
    if (!is_sink_role(m_ssr_role))
    {
        m_ssr_node_address_known = false;
        m_ssr_node_address = 0;
    }

    apply_cached_ssr_state_locked();
    Platform_unlock_ssr();
}

void wpc_ssr_on_role_set(app_role_t role)
{
    if (!Platform_lock_ssr())
    {
        return;
    }

    cache_role(role);
    if (!is_sink_role(m_ssr_role))
    {
        m_ssr_node_address_known = false;
        m_ssr_node_address = 0;
    }

    apply_cached_ssr_state_locked();
    Platform_unlock_ssr();
}

void wpc_ssr_on_node_address_known(app_addr_t node_address)
{
    if (!Platform_lock_ssr())
    {
        return;
    }

    m_ssr_node_address_known = true;
    m_ssr_node_address = node_address;
    if (m_ssr_role_known)
    {
        apply_cached_ssr_state_locked();
    }

    Platform_unlock_ssr();
}

bool wpc_ssr_get_first_hop_if_sink(app_addr_t dst_addr, uint32_t * first_hop_id)
{
    *first_hop_id = 0;

    if (!Platform_lock_ssr())
    {
        return false;
    }

    if (!m_ssr_role_known || !is_sink_role(m_ssr_role) || !m_ssr_node_address_known)
    {
        Platform_unlock_ssr();
        return false;
    }

    bool found = ssr_get_first_hop_locked(dst_addr, first_hop_id);
    Platform_unlock_ssr();
    return found;
}
