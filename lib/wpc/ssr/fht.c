/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 * First-Hop Table (FHT) implementation.
 *
 * Open-addressing hash table with linear probing and tombstone deletion.
 * Automatically grows (doubles) when the (occupied + tombstone) load exceeds
 * 0.7 of capacity. Rehashes on every fht_purge_expired call to eliminate
 * tombstones accumulated since the last purge.
 */
#define LOG_MODULE_NAME "fht"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "fht.h"    /* lib/wpc/include/fht.h via PRIVATE include path */
#include "platform.h"

#include <string.h>

#define FHT_INITIAL_CAPACITY 64
#define FHT_DEFAULT_VALIDITY 2400 /* seconds (40 min) */
#define FHT_LOAD_NUM         7
#define FHT_LOAD_DEN         10 /* max load factor = 0.7 */

typedef enum
{
    SLOT_EMPTY     = 0,
    SLOT_OCCUPIED  = 1,
    SLOT_TOMBSTONE = 2
} slot_state_t;

typedef struct
{
    uint32_t     node_id;
    uint32_t     first_hop_id;
    uint32_t     last_refresh;
    slot_state_t state;
} fht_slot_t;

struct fht
{
    fht_slot_t * slots;
    size_t       capacity;   /* always a power of 2 */
    size_t       count;      /* occupied entries */
    size_t       tombstones;
    uint32_t     validity_s;
};

/* ---- hash ---------------------------------------------------------------- */

static uint32_t hash_u32(uint32_t k)
{
    k ^= k >> 16;
    k *= UINT32_C(0x45d9f3b);
    k ^= k >> 16;
    k *= UINT32_C(0x45d9f3b);
    k ^= k >> 16;
    return k;
}

/* ---- probe --------------------------------------------------------------- */

/*
 * Linear-probe for node_id.
 * Sets *found = 1 and returns the slot index if the key exists.
 * Otherwise sets *found = 0 and returns the best insertion slot
 * (first tombstone along the probe path, or the first empty slot).
 */
static size_t probe(const fht_t * t, uint32_t node_id, int * found)
{
    size_t mask = t->capacity - 1;
    size_t idx  = hash_u32(node_id) & mask;
    size_t tomb = (size_t) -1;

    for (size_t i = 0; i < t->capacity; i++)
    {
        size_t          pos = (idx + i) & mask;
        const fht_slot_t * s = &t->slots[pos];

        if (s->state == SLOT_EMPTY)
        {
            *found = 0;
            return (tomb != (size_t) -1) ? tomb : pos;
        }
        if (s->state == SLOT_TOMBSTONE)
        {
            if (tomb == (size_t) -1)
                tomb = pos;
            continue;
        }
        if (s->node_id == node_id)
        {
            *found = 1;
            return pos;
        }
    }
    /* Should not happen with correct load-factor management. */
    *found = 0;
    return tomb;
}

/* ---- resize / rehash ----------------------------------------------------- */

static int resize(fht_t * t, size_t new_cap)
{
    fht_slot_t * old   = t->slots;
    size_t       old_n = t->capacity;

    fht_slot_t * fresh = (fht_slot_t *) Platform_malloc(new_cap * sizeof(*fresh));
    if (!fresh)
        return -1;
    memset(fresh, 0, new_cap * sizeof(*fresh));

    t->slots      = fresh;
    t->capacity   = new_cap;
    t->count      = 0;
    t->tombstones = 0;

    for (size_t i = 0; i < old_n; i++)
    {
        if (old[i].state != SLOT_OCCUPIED)
            continue;
        int    found;
        size_t pos    = probe(t, old[i].node_id, &found);
        t->slots[pos] = old[i];
        t->slots[pos].state = SLOT_OCCUPIED;
        t->count++;
    }

    Platform_free(old, old_n * sizeof(*old));
    return 0;
}

static int maybe_grow(fht_t * t)
{
    size_t used = t->count + t->tombstones;
    if (used * FHT_LOAD_DEN < t->capacity * FHT_LOAD_NUM)
        return 0;
    return resize(t, t->capacity * 2);
}

/* ---- public API ---------------------------------------------------------- */

fht_t * fht_create(void)
{
    fht_t * t = (fht_t *) Platform_malloc(sizeof(*t));
    if (!t)
        return NULL;
    memset(t, 0, sizeof(*t));

    t->slots = (fht_slot_t *) Platform_malloc(FHT_INITIAL_CAPACITY * sizeof(*t->slots));
    if (!t->slots)
    {
        Platform_free(t, sizeof(*t));
        return NULL;
    }
    memset(t->slots, 0, FHT_INITIAL_CAPACITY * sizeof(*t->slots));

    t->capacity   = FHT_INITIAL_CAPACITY;
    t->validity_s = FHT_DEFAULT_VALIDITY;
    return t;
}

void fht_destroy(fht_t * t)
{
    if (!t)
        return;
    Platform_free(t->slots, t->capacity * sizeof(*t->slots));
    Platform_free(t, sizeof(*t));
}

void fht_clear(fht_t * t)
{
    if (!t)
        return;

    memset(t->slots, 0, t->capacity * sizeof(*t->slots));
    t->count = 0;
    t->tombstones = 0;
}

void fht_set_validity(fht_t * t, uint32_t seconds)
{
    t->validity_s = seconds;
}

uint32_t fht_get_validity(const fht_t * t)
{
    return t->validity_s;
}

int fht_update_route(fht_t *   t,
                     uint32_t  node_id,
                     uint32_t  first_hop_id,
                     uint32_t  timestamp)
{
    if (maybe_grow(t) < 0)
        return -1;

    int    found;
    size_t pos = probe(t, node_id, &found);

    fht_slot_t * s = &t->slots[pos];
    if (found)
    {
        /* Reject stale updates. */
        if (timestamp <= s->last_refresh)
            return 0;
    }
    else
    {
        if (s->state == SLOT_TOMBSTONE)
            t->tombstones--;
        t->count++;
    }

    s->node_id      = node_id;
    s->first_hop_id = first_hop_id;
    s->last_refresh = timestamp;
    s->state        = SLOT_OCCUPIED;

    LOGD("FHT update: node=%u hop=%u ts=%u\n",
         node_id, first_hop_id, timestamp);

    return 0;
}

void fht_get_first_hop(const fht_t * t,
                       uint32_t      node_id,
                       uint32_t      now,
                       uint32_t *    first_hop_id)
{
    int    found;
    size_t pos = probe(t, node_id, &found);

    if (found)
    {
        const fht_slot_t * s = &t->slots[pos];
        if (now - s->last_refresh < t->validity_s)
        {
            *first_hop_id = s->first_hop_id;
            return;
        }
    }

    *first_hop_id = 0;
}

void fht_purge_expired(fht_t * t, uint32_t now)
{
    for (size_t i = 0; i < t->capacity; i++)
    {
        fht_slot_t * s = &t->slots[i];
        if (s->state == SLOT_OCCUPIED && now - s->last_refresh >= t->validity_s)
        {
            s->state = SLOT_TOMBSTONE;
            t->count--;
            t->tombstones++;
        }
    }

    /* Rehash to eliminate accumulated tombstones. */
    if (t->tombstones > 0)
        resize(t, t->capacity);
}
