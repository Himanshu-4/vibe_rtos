/**
 * @file lib/heap/heap_best_fit.c
 * @brief Best-fit heap allocator for VibeRTOS.
 *
 * Algorithm:
 *   alloc — scan the entire free list and pick the smallest block whose
 *            size >= requested.  Split the remainder into a new free block.
 *            Reduces external fragmentation at the cost of a full O(n) scan.
 *   free  — mark the block free and coalesce with physical neighbours.
 *
 * Complexity:  alloc O(n),  free O(1)
 */

#include "vibe/heap.h"
#include "heap_priv.h"

#if defined(CONFIG_HEAP_MEM) && defined(CONFIG_HEAP_ALLOCATOR_BEST_FIT)

#ifndef CONFIG_HEAP_SIZE
#define CONFIG_HEAP_SIZE 4096
#endif

/* -------------------------------------------------------------------------
 * Pool storage
 * ---------------------------------------------------------------------- */

static uint8_t _Alignas(HEAP_ALIGN) g_heap_pool[CONFIG_HEAP_SIZE];

static heap_state_t g_st = {
    .pool        = g_heap_pool,
    .pool_size   = CONFIG_HEAP_SIZE,
    .head        = NULL,
    .lock        = VIBE_SPINLOCK_INIT,
    .used_bytes  = 0,
    .alloc_count = 0,
    .initialized = false,
};

/* -------------------------------------------------------------------------
 * vibe_heap_alloc — best-fit search
 * ---------------------------------------------------------------------- */

void *vibe_heap_alloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }
    size = HEAP_ALIGN_UP(size);

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_st.lock);

    if (!g_st.initialized) {
        heap_pool_init(&g_st);
    }

    hdr_t *best = NULL;
    for (hdr_t *b = g_st.head; b != NULL; b = b->next) {
        if (b->magic == HEAP_MAGIC_FREE && b->size >= size) {
            if (best == NULL || b->size < best->size) {
                best = b;
                if (best->size == size) {
                    break; /* perfect fit — stop early */
                }
            }
        }
    }

    void *ret = NULL;
    if (best != NULL) {
        heap_split(best, size);
        best->magic = HEAP_MAGIC_USED;
        g_st.used_bytes  += best->size;
        g_st.alloc_count++;
        ret = ptr_from_hdr(best);
    }

    vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
    return ret;
}

/* -------------------------------------------------------------------------
 * vibe_heap_free
 * ---------------------------------------------------------------------- */

void vibe_heap_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    hdr_t *blk = hdr_from_ptr(ptr);

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_st.lock);

#if defined(CONFIG_HEAP_CORRUPTION_CHECK)
    if (blk->magic != HEAP_MAGIC_USED) {
        vibe_printk("heap: bad free %p (magic=0x%x)\n", ptr, (unsigned)blk->magic);
        vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
        return;
    }
#endif

    g_st.used_bytes  -= blk->size;
    g_st.alloc_count--;
    blk->magic = HEAP_MAGIC_FREE;
    heap_coalesce(blk);

    vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
}

/* -------------------------------------------------------------------------
 * vibe_heap_realloc
 * ---------------------------------------------------------------------- */

void *vibe_heap_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return vibe_heap_alloc(size);
    }
    if (size == 0U) {
        vibe_heap_free(ptr);
        return NULL;
    }

    hdr_t *blk      = hdr_from_ptr(ptr);
    size_t old_size = blk->size;
    size_t new_size = HEAP_ALIGN_UP(size);

    if (new_size <= old_size) {
        return ptr;
    }

    void *np = vibe_heap_alloc(size);
    if (np == NULL) {
        return NULL;
    }
    memcpy(np, ptr, old_size < size ? old_size : size);
    vibe_heap_free(ptr);
    return np;
}

/* -------------------------------------------------------------------------
 * vibe_heap_calloc
 * ---------------------------------------------------------------------- */

void *vibe_heap_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0U || size == 0U) {
        return NULL;
    }
    if (nmemb > (SIZE_MAX / size)) {
        return NULL;
    }
    size_t total = nmemb * size;
    void  *p     = vibe_heap_alloc(total);
    if (p != NULL) {
        memset(p, 0, total);
    }
    return p;
}

/* -------------------------------------------------------------------------
 * vibe_heap_stats
 * ---------------------------------------------------------------------- */

void vibe_heap_stats(vibe_heap_stats_t *s)
{
    if (s == NULL) {
        return;
    }
#if defined(CONFIG_HEAP_STATS)
    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_st.lock);
    heap_walk_stats(&g_st, s);
    vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
#else
    memset(s, 0, sizeof(*s));
#endif
}

#endif /* CONFIG_HEAP_MEM && CONFIG_HEAP_ALLOCATOR_BEST_FIT */
