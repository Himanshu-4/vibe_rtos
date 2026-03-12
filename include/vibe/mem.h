#ifndef VIBE_MEM_H
#define VIBE_MEM_H

/**
 * @file mem.h
 * @brief Memory management API for VibeRTOS.
 *
 * Provides:
 *  - Fixed-size memory pools (deterministic O(1) alloc/free)
 *  - Optional heap (first-fit, guarded by CONFIG_HEAP_MEM)
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Fixed-size memory pool
 * --------------------------------------------------------------------- */

typedef struct vibe_mem_pool {
    size_t       block_size;   /**< Size of each block in bytes. */
    uint32_t     num_blocks;   /**< Total number of blocks. */
    uint32_t     free_count;   /**< Number of currently free blocks. */
    void        *blocks;       /**< Pointer to the raw block storage. */
    void        *free_list;    /**< Head of the singly-linked free list. */
    vibe_list_t  wait_list;    /**< Threads waiting for a free block. */
} vibe_mem_pool_t;

/** Alias: memory slab = fixed-size pool. */
typedef vibe_mem_pool_t vibe_slab_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_MEM_POOL_DEFINE — statically define a fixed-size memory pool.
 *
 * @param name        C identifier.
 * @param block_sz    Size of each block in bytes.
 * @param num         Number of blocks.
 */
#define VIBE_MEM_POOL_DEFINE(name, block_sz, num)                    \
    static uint8_t _##name##_buf[(block_sz) * (num)]                 \
        __attribute__((aligned(8)));                                  \
    vibe_mem_pool_t name = {                                         \
        .block_size = (block_sz),                                    \
        .num_blocks = (num),                                         \
        .free_count = (num),                                         \
        .blocks     = _##name##_buf,                                 \
        .free_list  = NULL,                                          \
    }

/* -----------------------------------------------------------------------
 * Memory pool API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a memory pool.
 *
 * Partitions the block storage into block_size chunks and builds the
 * initial free list.
 *
 * @param pool        Pool to initialise.
 * @param buffer      Pre-allocated storage of size (block_size * num_blocks).
 * @param block_size  Size of each block in bytes.
 * @param num_blocks  Number of blocks.
 * @return            VIBE_OK or VIBE_EINVAL.
 */
vibe_err_t vibe_mem_pool_init(vibe_mem_pool_t *pool,
                               void            *buffer,
                               size_t           block_size,
                               uint32_t         num_blocks);

/**
 * @brief Allocate one block from the pool, blocking if all are in use.
 *
 * @param pool     Memory pool.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or tick count.
 * @return         Pointer to the allocated block, or NULL on timeout/error.
 */
void *vibe_mem_pool_alloc(vibe_mem_pool_t *pool, vibe_tick_t timeout);

/**
 * @brief Return a block to the pool.
 *
 * Wakes the highest-priority waiter if one is blocked.
 *
 * @param pool  Memory pool.
 * @param ptr   Pointer returned by vibe_mem_pool_alloc().
 * @return      VIBE_OK or VIBE_EINVAL (ptr out of range).
 */
vibe_err_t vibe_mem_pool_free(vibe_mem_pool_t *pool, void *ptr);

/**
 * @brief Return the number of free blocks remaining in the pool.
 *
 * @param pool  Memory pool.
 * @return      Free block count.
 */
uint32_t vibe_mem_pool_free_count(const vibe_mem_pool_t *pool);

/* -----------------------------------------------------------------------
 * Dynamic heap — full API lives in vibe/heap.h
 * --------------------------------------------------------------------- */

#include "vibe/heap.h"

#ifdef __cplusplus
}
#endif

#endif /* VIBE_MEM_H */
