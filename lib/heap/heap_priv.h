#ifndef HEAP_PRIV_H
#define HEAP_PRIV_H

/**
 * @file lib/heap/heap_priv.h
 * @brief Shared private definitions for first-fit and best-fit backends.
 *
 * Block layout in memory (physically contiguous):
 *
 *   +------------------+  <- block pointer (hdr_t *)
 *   | prev  (4 bytes)  |  previous physical block, NULL if first
 *   | size  (4 bytes)  |  payload bytes (excl. header)
 *   | used  (1 byte)   |  1 = allocated, 0 = free
 *   | _pad  (3 bytes)  |  alignment padding
 *   +------------------+  <- user pointer returned to caller
 *   | payload ...      |
 *   +------------------+
 *
 * Minimum block size:  sizeof(hdr_t) + HEAP_ALIGN  (ensures a split always
 * leaves a usable free block — never a runt smaller than one alignment unit).
 */

#include "vibe/types.h"
#include "vibe/spinlock.h"
#include "vibe/sys/printk.h"
#include <string.h>

/* --------------------------------------------------------------------- */

#define HEAP_ALIGN          8U
#define HEAP_ALIGN_MASK     (HEAP_ALIGN - 1U)
#define HEAP_ALIGN_UP(x)    (((size_t)(x) + HEAP_ALIGN_MASK) & ~HEAP_ALIGN_MASK)

/* Block magic values for corruption detection */
#define HEAP_MAGIC_FREE     0xFEEDF00DU
#define HEAP_MAGIC_USED     0xA110CA7EU

/* --------------------------------------------------------------------- */

typedef struct hdr {
    struct hdr *prev;   /**< Previous physical block (NULL = first block) */
    struct hdr *next;   /**< Next physical block     (NULL = last block)  */
    size_t      size;   /**< Payload size in bytes (excluding this header)*/
    uint32_t    magic;  /**< HEAP_MAGIC_FREE or HEAP_MAGIC_USED           */
} hdr_t;

#define HDR_SIZE        HEAP_ALIGN_UP(sizeof(hdr_t))
#define BLK_MIN_PAYLOAD HEAP_ALIGN
#define BLK_MIN_SIZE    (HDR_SIZE + BLK_MIN_PAYLOAD)

/** Convert a user pointer back to its block header. */
static inline hdr_t *hdr_from_ptr(void *ptr)
{
    return (hdr_t *)((uint8_t *)ptr - HDR_SIZE);
}

/** Get the user pointer from a block header. */
static inline void *ptr_from_hdr(hdr_t *h)
{
    return (void *)((uint8_t *)h + HDR_SIZE);
}

/* --------------------------------------------------------------------- */

/** Shared heap state (used by both FF and BF backends). */
typedef struct {
    uint8_t           *pool;
    size_t             pool_size;
    hdr_t             *head;       /**< First physical block             */
    vibe_spinlock_t    lock;
    size_t             used_bytes;
    size_t             alloc_count;
    bool               initialized;
} heap_state_t;

/** One-time initialisation of the pool. Caller holds the lock. */
static inline void heap_pool_init(heap_state_t *st)
{
    hdr_t *blk   = (hdr_t *)st->pool;
    blk->prev    = NULL;
    blk->next    = NULL;
    blk->size    = st->pool_size - HDR_SIZE;
    blk->magic   = HEAP_MAGIC_FREE;
    st->head      = blk;
    st->initialized = true;
}

/**
 * Split block @p blk so that it holds exactly @p size payload bytes.
 * The remainder (if large enough) becomes a new free block inserted
 * immediately after @p blk in the physical chain.
 */
static inline void heap_split(hdr_t *blk, size_t size)
{
    size_t leftover = blk->size - size;
    if (leftover < BLK_MIN_SIZE) {
        return; /* not enough room — give the whole block */
    }
    hdr_t *nxt     = (hdr_t *)((uint8_t *)blk + HDR_SIZE + size);
    nxt->prev      = blk;
    nxt->next      = blk->next;
    nxt->size      = leftover - HDR_SIZE;
    nxt->magic     = HEAP_MAGIC_FREE;
    if (blk->next) {
        blk->next->prev = nxt;
    }
    blk->next = nxt;
    blk->size = size;
}

/**
 * Coalesce @p blk with adjacent free blocks (prev + next) in physical order.
 * Returns the surviving (merged) block pointer.
 */
static inline hdr_t *heap_coalesce(hdr_t *blk)
{
    /* Merge with next if free */
    if (blk->next && blk->next->magic == HEAP_MAGIC_FREE) {
        hdr_t *nxt  = blk->next;
        blk->size  += HDR_SIZE + nxt->size;
        blk->next   = nxt->next;
        if (nxt->next) {
            nxt->next->prev = blk;
        }
    }
    /* Merge with prev if free */
    if (blk->prev && blk->prev->magic == HEAP_MAGIC_FREE) {
        hdr_t *prv  = blk->prev;
        prv->size  += HDR_SIZE + blk->size;
        prv->next   = blk->next;
        if (blk->next) {
            blk->next->prev = prv;
        }
        blk = prv;
    }
    return blk;
}

#if defined(CONFIG_HEAP_STATS)
/** Fill vibe_heap_stats_t by walking the physical block chain. */
static inline void heap_walk_stats(heap_state_t *st, vibe_heap_stats_t *out)
{
    out->total        = st->pool_size - HDR_SIZE;
    out->used         = 0;
    out->free         = 0;
    out->largest_free = 0;
    out->alloc_count  = 0;
    out->free_count   = 0;

    for (hdr_t *b = st->head; b != NULL; b = b->next) {
        if (b->magic == HEAP_MAGIC_USED) {
            out->used += b->size;
            out->alloc_count++;
        } else {
            out->free += b->size;
            out->free_count++;
            if (b->size > out->largest_free) {
                out->largest_free = b->size;
            }
        }
    }
}
#endif /* CONFIG_HEAP_STATS */

#endif /* HEAP_PRIV_H */
