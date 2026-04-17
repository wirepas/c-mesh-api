#ifndef WPC_SSR_H__
#define WPC_SSR_H__

#include <stdbool.h>
#include <stdint.h>

#include "wpc.h"

void wpc_ssr_reset(void);
void wpc_ssr_init(void);
void wpc_ssr_close(void);
bool wpc_ssr_set_enabled(bool enabled);
bool wpc_ssr_reset_routes(void);
void wpc_ssr_on_role_read(app_role_t role);
void wpc_ssr_on_role_set(app_role_t role);
void wpc_ssr_on_node_address_known(app_addr_t node_address);
bool wpc_ssr_get_first_hop_if_sink(app_addr_t dst_addr, uint32_t * first_hop_id);

#endif /* WPC_SSR_H__ */
