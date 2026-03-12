/**
 * @file kernel/heap.c
 * @brief First-fit dynamic heap allocator for VibeRTOS.
 *
 * Only compiled when CONFIG_HEAP_MEM is defined.
 * The heap is a statically allocated byte array partitioned into a
 * singly-linked list of free/used blocks.
 */

#include "vibe/mem.h"
#include "vibe/spinlock.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include <string.h>

#ifdef CONFIG_HEAP_MEM

#ifndef CONFIG_HEAP_SIZE
#define CONFIG_HEAP_SIZE  4096
#endif

/* -----------------------------------------------------------------------
 * Heap block header
 * --------------------------------------------------------------------- */

typedef struct _heap_block {
    size_t              size;   /**< Payload size (excluding this header). */
    bool                used;   /**< True if allocated, false if free. */
    struct _heap_block *next;   /**< Next block in the list. */
} _heap_block_t;

/* Minimum alignment for returned pointers. */
#define HEAP_ALIGN  8U

/* -----------------------------------------------------------------------
 * Heap storage
 * --------------------------------------------------------------------- */

static uint8_t      g_heap_mem[CONFIG_HEAP_SIZE] __attribute__((aligned(8)));
static _heap_block_t *g_heap_head = NULL;
static vibe_spinlock_t g_heap_lock = VIBE_SPINLOCK_INIT;

static bool g_heap_initialized = false;

static void _heap_init(void)
{
    g_heap_head       = (_heap_block_t *)g_heap_mem;
    g_heap_head->size = CONFIG_HEAP_SIZE - sizeof(_heap_block_t);
    g_heap_head->used = false;
    g_heap_head->next = NULL;
    g_heap_initialized = true;
}

/* -----------------------------------------------------------------------
 * vibe_heap_alloc()
 * --------------------------------------------------------------------- */

void *vibe_heap_alloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }

    /* Round up to alignment. */
    size = ALIGN_UP(size, HEAP_ALIGN);

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_heap_lock);

    if (!g_heap_initialized) {
        _heap_init();
    }

    /* First-fit search. */
    _heap_block_t *blk = g_heap_head;
    while (blk != NULL) {
        if (!blk->used && blk->size >= size) {
            /* Split block if there is enough space for a new free header + min payload. */
            size_t remaining = blk->size - size;
            if (remaining > sizeof(_heap_block_t) + HEAP_ALIGN) {
                _heap_block_t *new_blk = (_heap_block_t *)((uint8_t *)(blk + 1) + size);
                new_blk->size = remaining - sizeof(_heap_block_t);
                new_blk->used = false;
                new_blk->next = blk->next;
                blk->next     = new_blk;
                blk->size     = size;
            }
            blk->used = true;
            vibe_spinlock_unlock_irqrestore(&g_heap_lock, key);
            return (void *)(blk + 1);
        }
        blk = blk->next;
    }

    vibe_spinlock_unlock_irqrestore(&g_heap_lock, key);
    return NULL; /* Out of memory */
}

/* -----------------------------------------------------------------------
 * vibe_heap_free()
 * --------------------------------------------------------------------- */

void vibe_heap_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    _heap_block_t *blk = (_heap_block_t *)ptr - 1;

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_heap_lock);

    blk->used = false;

    /* Coalesce with next block if it is free. */
    while (blk->next != NULL && !blk->next->used) {
        blk->size += sizeof(_heap_block_t) + blk->next->size;
        blk->next  = blk->next->next;
    }

    vibe_spinlock_unlock_irqrestore(&g_heap_lock, key);
}

/* -----------------------------------------------------------------------
 * vibe_heap_stats()
 * --------------------------------------------------------------------- */

void vibe_heap_stats(size_t *used_out, size_t *free_out)
{
    size_t used = 0U;
    size_t free = 0U;

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_heap_lock);

    _heap_block_t *blk = g_heap_head;
    while (blk != NULL) {
        if (blk->used) {
            used += blk->size;
        } else {
            free += blk->size;
        }
        blk = blk->next;
    }

    vibe_spinlock_unlock_irqrestore(&g_heap_lock, key);

    if (used_out) { *used_out = used; }
    if (free_out) { *free_out = free; }
}

#endif /* CONFIG_HEAP_MEM */
