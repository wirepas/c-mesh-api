/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 * First-Hop Table (FHT) for Selective Source Routing bookkeeping.
 *
 * The table stores the first hop learned for each registered node and is used
 * to build SSR downlink transmissions.
 */
#ifndef FHT_H__
#define FHT_H__

#include <stdint.h>

/** Opaque first-hop table handle. */
typedef struct fht fht_t;

/** Create a new first-hop table. Returns NULL on allocation failure. */
fht_t *fht_create(void);

/** Destroy the table and free all memory. Safe to call with NULL. */
void fht_destroy(fht_t *table);

/** Remove all entries from the table while keeping its allocation. */
void fht_clear(fht_t *table);

/** Set the validity timer applied to all entries (seconds). */
void fht_set_validity(fht_t *table, uint32_t seconds);

/** Get the current validity timer (seconds). */
uint32_t fht_get_validity(const fht_t *table);

/**
 * \brief Insert or update a route entry.
 *
 * \param table        First-hop table.
 * \param node_id      Target node.
 * \param first_hop_id Next hop toward that node (source routing anchor).
 * \param timestamp    Caller-provided time of this update (seconds).
 * \return 0 on success, -1 on allocation failure.
 *
 * \note If an entry for node_id already exists and timestamp is not newer
 *       than the stored last_refresh, the update is silently ignored.
 */
int fht_update_route(fht_t *table,
                     uint32_t node_id,
                     uint32_t first_hop_id,
                     uint32_t timestamp);

/**
 * \brief Look up the first hop for a node.
 *
 * If the entry is missing or expired, the output parameter is set to 0.
 *
 * \param table        First-hop table.
 * \param node_id      Target node to look up.
 * \param now          Caller-provided current time (seconds).
 * \param first_hop_id [out] First hop address, or 0 if not found/expired.
 */
void fht_get_first_hop(const fht_t *table,
                       uint32_t node_id,
                       uint32_t now,
                       uint32_t *first_hop_id);

/**
 * \brief Remove all expired entries and rehash the table.
 *
 * \param now Caller-provided current time (seconds).
 */
void fht_purge_expired(fht_t *table, uint32_t now);

#endif /* FHT_H__ */
