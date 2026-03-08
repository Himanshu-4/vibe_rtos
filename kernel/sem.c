/**
 * @file kernel/sem.c
 * @brief Counting semaphore implementation for VibeRTOS.
 */

#include "vibe/sem.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/list.h"
#include <string.h>

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

vibe_err_t vibe_sem_init(vibe_sem_t *sem, uint32_t initial, uint32_t max)
{
    if (sem == NULL) {
        return VIBE_EINVAL;
    }
    if (max > 0U && initial > max) {
        return VIBE_EINVAL;
    }
    sem->count     = initial;
    sem->max_count = max;
    vibe_list_init(&sem->wait_list);
    return VIBE_OK;
}

vibe_err_t vibe_sem_take(vibe_sem_t *sem, vibe_tick_t timeout)
{
    if (sem == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (sem->count > 0U) {
        sem->count--;
        arch_irq_unlock(key);
        return VIBE_OK;
    }

    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }

    /* Block until a give() wakes us or we time out. */
    vibe_thread_t *self = vibe_thread_self();
    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;

    vibe_list_append(&sem->wait_list, &self->wait_node);

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* Woke up — check if we successfully took it. */
    key = arch_irq_lock();
    bool got_it = (self->state != VIBE_THREAD_SLEEPING); /* crude check */
    arch_irq_unlock(key);

    return got_it ? VIBE_OK : VIBE_ETIMEOUT;
}

vibe_err_t vibe_sem_give(vibe_sem_t *sem)
{
    if (sem == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    vibe_list_node_t *head = vibe_list_get_head(&sem->wait_list);
    if (head != NULL) {
        /* Wake the waiter — hand the token directly to it. */
        vibe_thread_t *waiter = CONTAINER_OF(head, vibe_thread_t, wait_node);
        _vibe_sched_enqueue(waiter);
        arch_irq_unlock(key);
        _vibe_sched_reschedule();
        return VIBE_OK;
    }

    /* No waiters — increment count up to max. */
    if (sem->max_count > 0U && sem->count >= sem->max_count) {
        arch_irq_unlock(key);
        return VIBE_EOVERFLOW;
    }

    sem->count++;
    arch_irq_unlock(key);
    return VIBE_OK;
}

uint32_t vibe_sem_count(const vibe_sem_t *sem)
{
    return sem ? sem->count : 0U;
}

void vibe_sem_reset(vibe_sem_t *sem)
{
    if (sem == NULL) {
        return;
    }
    vibe_irq_key_t key = arch_irq_lock();
    sem->count = 0U;
    arch_irq_unlock(key);
}
