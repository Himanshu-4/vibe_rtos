#ifndef VIBE_HEAP_H
#define VIBE_HEAP_H

/**
 * @file vibe/heap.h
 * @brief Dynamic heap allocator API for VibeRTOS.
 *
 * Three allocator backends, selected at build time via Kconfig:
 *
 *   CONFIG_HEAP_ALLOCATOR_FIRST_FIT   — simple sequential search, low overhead
 *   CONFIG_HEAP_ALLOCATOR_BEST_FIT    — minimises fragmentation, O(n) search
 *   CONFIG_HEAP_ALLOCATOR_TLSF        — O(1) real-time allocator (default)
 *
 * All backends are thread-safe (IRQ-safe spinlock).
 * The heap lives in a statically allocated array of CONFIG_HEAP_SIZE bytes.
 *
 * Enable with CONFIG_HEAP_MEM=y.
 */

#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_HEAP_MEM

/* -------------------------------------------------------------------------
 * Core allocator
 * ---------------------------------------------------------------------- */

/**
 * @brief Allocate at least @p size bytes from the heap.
 *
 * Returned pointer is aligned to HEAP_ALIGN (8) bytes.
 * Returns NULL if out of memory or size == 0.
 */
void *vibe_heap_alloc(size_t size);

/**
 * @brief Free a heap allocation.
 *
 * @p ptr must have been returned by vibe_heap_alloc(), vibe_heap_calloc(),
 * or vibe_heap_realloc(). Passing NULL is a no-op.
 */
void vibe_heap_free(void *ptr);

/**
 * @brief Resize an existing allocation.
 *
 * Behaviour matches POSIX realloc():
 *   - If ptr == NULL, equivalent to vibe_heap_alloc(size).
 *   - If size == 0,   equivalent to vibe_heap_free(ptr), returns NULL.
 *   - On failure returns NULL; original block is NOT freed.
 */
void *vibe_heap_realloc(void *ptr, size_t size);

/**
 * @brief Allocate @p nmemb × @p size bytes, zero-initialised.
 *
 * Returns NULL on overflow or out of memory.
 */
void *vibe_heap_calloc(size_t nmemb, size_t size);

/* -------------------------------------------------------------------------
 * Diagnostics
 * ---------------------------------------------------------------------- */

/** Heap usage snapshot. */
typedef struct {
    size_t total;         /**< Total heap size (bytes)              */
    size_t used;          /**< Bytes currently allocated            */
    size_t free;          /**< Bytes currently free                 */
    size_t largest_free;  /**< Largest single free block (bytes)    */
    size_t alloc_count;   /**< Live allocation count                */
    size_t free_count;    /**< Free block count                     */
} vibe_heap_stats_t;

/**
 * @brief Fill @p s with current heap statistics.
 */
void vibe_heap_stats(vibe_heap_stats_t *s);

/**
 * @brief Legacy two-value stats shim (matches old kernel/heap.c signature).
 */
static inline void vibe_heap_stats2(size_t *used, size_t *free_bytes)
{
    vibe_heap_stats_t s;
    vibe_heap_stats(&s);
    if (used)       { *used       = s.used; }
    if (free_bytes) { *free_bytes = s.free; }
}

/* -------------------------------------------------------------------------
 * Convenience aliases (malloc / free style)
 * ---------------------------------------------------------------------- */

#define vibe_malloc(sz)        vibe_heap_alloc(sz)
#define vibe_free(p)           vibe_heap_free(p)
#define vibe_calloc(n, sz)     vibe_heap_calloc((n), (sz))
#define vibe_realloc(p, sz)    vibe_heap_realloc((p), (sz))

#endif /* CONFIG_HEAP_MEM */

#ifdef __cplusplus
}
#endif

#endif /* VIBE_HEAP_H */
