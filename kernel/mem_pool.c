/**
 * @file kernel/mem_pool.c
 * @brief Fixed-size memory pool (slab allocator) for VibeRTOS.
 */

#include "vibe/mem.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/list.h"
#include <string.h>

#include "vibe/arch.h"

/* Each free block stores a pointer to the next free block at its start. */
typedef struct _free_block {
    struct _free_block *next;
} _free_block_t;

vibe_err_t vibe_mem_pool_init(vibe_mem_pool_t *pool,
                               void            *buffer,
                               size_t           block_size,
                               uint32_t         num_blocks)
{
    if (pool == NULL || buffer == NULL || block_size == 0U || num_blocks == 0U) {
        return VIBE_EINVAL;
    }
    /* block_size must be at least sizeof(pointer) for the free list. */
    if (block_size < sizeof(void *)) {
        block_size = sizeof(void *);
    }

    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_count = num_blocks;
    pool->blocks     = buffer;
    vibe_list_init(&pool->wait_list);

    /* Build the free list by chaining blocks together. */
    uint8_t *p = (uint8_t *)buffer;
    _free_block_t *prev_blk = NULL;
    for (uint32_t i = 0; i < num_blocks; i++) {
        _free_block_t *blk = (_free_block_t *)p;
        if (i == 0U) {
            pool->free_list = blk;
        }
        if (prev_blk != NULL) {
            prev_blk->next = blk;
        }
        blk->next  = NULL;
        prev_blk   = blk;
        p         += block_size;
    }

    return VIBE_OK;
}

void *vibe_mem_pool_alloc(vibe_mem_pool_t *pool, vibe_tick_t timeout)
{
    if (pool == NULL) {
        return NULL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (pool->free_list != NULL) {
        _free_block_t *blk = (_free_block_t *)pool->free_list;
        pool->free_list    = blk->next;
        pool->free_count--;
        arch_irq_unlock(key);
        /* Optionally zero on alloc. */
        memset(blk, 0, pool->block_size);
        return blk;
    }

    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return NULL;
    }

    /* Block until a block is freed or timeout. */
    vibe_thread_t *self = vibe_thread_self();
    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;
    vibe_list_append(&pool->wait_list, &self->wait_node);

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* Re-try after wakeup. */
    key = arch_irq_lock();
    if (pool->free_list != NULL) {
        _free_block_t *blk = (_free_block_t *)pool->free_list;
        pool->free_list    = blk->next;
        pool->free_count--;
        arch_irq_unlock(key);
        memset(blk, 0, pool->block_size);
        return blk;
    }
    arch_irq_unlock(key);
    return NULL;
}

vibe_err_t vibe_mem_pool_free(vibe_mem_pool_t *pool, void *ptr)
{
    if (pool == NULL || ptr == NULL) {
        return VIBE_EINVAL;
    }

    /* Validate pointer is within pool range. */
    uint8_t *base = (uint8_t *)pool->blocks;
    uint8_t *end  = base + pool->num_blocks * pool->block_size;
    if ((uint8_t *)ptr < base || (uint8_t *)ptr >= end) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    _free_block_t *blk = (_free_block_t *)ptr;
    blk->next       = (_free_block_t *)pool->free_list;
    pool->free_list  = blk;
    pool->free_count++;

    /* Wake the highest-priority waiter. */
    vibe_list_node_t *wn = vibe_list_get_head(&pool->wait_list);
    if (wn != NULL) {
        vibe_thread_t *t = CONTAINER_OF(wn, vibe_thread_t, wait_node);
        _vibe_sched_enqueue(t);
    }

    arch_irq_unlock(key);
    _vibe_sched_reschedule();
    return VIBE_OK;
}

uint32_t vibe_mem_pool_free_count(const vibe_mem_pool_t *pool)
{
    return pool ? pool->free_count : 0U;
}
