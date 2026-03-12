/**
 * @file lib/heap/heap_first_fit.c
 * @brief First-fit heap allocator for VibeRTOS.
 *
 * Algorithm:
 *   alloc — walk the physical block chain from the head and return the
 *            first free block that is large enough; split any surplus.
 *   free  — mark the block free, then coalesce with physical neighbours.
 *
 * Complexity:  alloc O(n),  free O(1)  (n = block count)
 * Overhead:    one hdr_t (16 bytes) per allocation + pool-level state
 */

#include "vibe/heap.h"
#include "heap_priv.h"

#if defined(CONFIG_HEAP_MEM) && defined(CONFIG_HEAP_ALLOCATOR_FIRST_FIT)

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
 * vibe_heap_alloc
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

    void *ret = NULL;
    for (hdr_t *b = g_st.head; b != NULL; b = b->next) {
        if (b->magic == HEAP_MAGIC_FREE && b->size >= size) {
            heap_split(b, size);
            b->magic = HEAP_MAGIC_USED;
            g_st.used_bytes  += b->size;
            g_st.alloc_count++;
            ret = ptr_from_hdr(b);
            break;
        }
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

    if (blk->magic != HEAP_MAGIC_USED) {
        /* Double-free or corruption — print and bail */
        vibe_printk("heap: bad free %p (magic=0x%x)\n", ptr, (unsigned)blk->magic);
        vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
        return;
    }

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
        /* Optionally split the tail — for now just reuse. */
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
    /* Overflow check */
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

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_st.lock);
    heap_walk_stats(&g_st, s);
    vibe_spinlock_unlock_irqrestore(&g_st.lock, key);
}

#endif /* CONFIG_HEAP_MEM && CONFIG_HEAP_ALLOCATOR_FIRST_FIT */
