/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 * SSR (Selective Source Routing) integration layer for c-mesh-api.
 *
 * This module maintains a First-Hop Table (FHT) fed by MSAP SSR registration
 * indications received from the connected sink.
 *
 * Typical lifecycle:
 *   1. ssr_init()  - called once during WPC_initialize().
 *   2. ssr_on_registration() - called by the MSAP handler each time a sink
 *      forwards an SSR registration packet from a mesh node.
 *   3. ssr_get_first_hop()  - queried by the send path or tests to retrieve
 *      the routing anchor for a node.
 *   4. ssr_purge_expired()  - called periodically from the platform dispatch
 *      thread (every DISPATCH_WAKEUP_TIMEOUT_S seconds).
 *   5. ssr_deinit() - optional cleanup (primarily for tests).
 */
#ifndef SSR_H__
#define SSR_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * \brief Callback invoked each time a new SSR registration is learned.
 *
 * \param source_address  Node address that registered.
 * \param first_hop_id    First-hop anchor (source routing ID).
 * \param delay_hp        End-to-end delay in 1/1024 second units.
 */
typedef void (*onSsrRegistration_cb_f)(uint32_t source_address,
                                       uint32_t first_hop_id,
                                       uint32_t delay_hp);

/**
 * \brief Initialise the SSR module and allocate the First-Hop Table.
 *
 * Safe to call multiple times; subsequent calls are no-ops if already
 * initialised.
 */
void ssr_init(void);

/**
 * \brief Release all SSR resources.
 *
 * After this call the module is in the same state as before ssr_init().
 * Primarily intended for unit-test teardown.
 */
void ssr_deinit(void);

/**
 * \brief Configure the sink address accepted by SSR registrations.
 *
 * Registrations learned from any other sink address are discarded.
 *
 * \param sink_address Node address of the connected sink.
 */
void ssr_set_sink_address(uint32_t sink_address);

/**
 * \brief Disable SSR sink-address filtering until a sink address is set again.
 *
 * After this call all incoming registrations are discarded.
 */
void ssr_clear_sink_address(void);

/**
 * \brief Clear all learned SSR routes while keeping the module initialised.
 */
void ssr_reset_routes(void);

/**
 * \brief Process an incoming SSR registration from a sink.
 *
 * Called by the MSAP indication handler whenever
 * MSAP_SSR_REGISTRATION_INDICATION is received.
 *
 * \param source_address  Node that sent the registration packet.
 * \param source_routing_id First-hop anchor chosen by the mesh for that node.
 * \param sink_address    Sink that received the registration.
 * \param delay_hp        End-to-end delay in 1/1024 second units; used to
 *                        back-calculate the generation time of the packet.
 */
void ssr_on_registration(uint32_t source_address,
                         uint32_t source_routing_id,
                         uint32_t sink_address,
                         uint32_t delay_hp);

/**
 * \brief Query the first hop to reach a destination node.
 *
 * \param dest           Destination node address.
 * \param first_hop_id   [out] First-hop anchor address; 0 if no valid route.
 * \return true if a valid, non-expired route was found.
 */
bool ssr_get_first_hop(uint32_t  dest,
                       uint32_t *first_hop_id);

/**
 * \brief Purge expired entries from the First-Hop Table.
 *
 * Should be called periodically (e.g. from the platform dispatch thread).
 */
void ssr_purge_expired(void);

/**
 * \brief Register a callback to be notified on each new SSR registration.
 *
 * Only one callback can be registered at a time.
 *
 * \param cb  Callback function.
 * \return true on success, false if a callback is already registered.
 */
bool ssr_register_for_registration(onSsrRegistration_cb_f cb);

/**
 * \brief Unregister the SSR registration callback.
 *
 * \return true on success, false if no callback was registered.
 */
bool ssr_unregister_from_registration(void);

#endif /* SSR_H__ */
