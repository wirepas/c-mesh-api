#ifndef SSR_BACKEND_H__
#define SSR_BACKEND_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * Private SSR backend interface.
 *
 * These entry points assume the caller already holds Platform_lock_ssr().
 * They are used by the WPC integration layer to update/query backend routing
 * state atomically with its own cached sink eligibility state.
 */
void ssr_set_sink_address_locked(uint32_t sink_address);
void ssr_clear_sink_address_locked(void);
void ssr_reset_routes_locked(void);
bool ssr_get_first_hop_locked(uint32_t dest, uint32_t * first_hop_id);

#endif /* SSR_BACKEND_H__ */
