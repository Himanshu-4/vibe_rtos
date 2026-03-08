/**
 * @file kernel/mutex.c
 * @brief Priority-inheritance mutex implementation for VibeRTOS.
 */

#include "vibe/mutex.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/list.h"
#include <string.h>

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

vibe_err_t vibe_mutex_init(vibe_mutex_t *mutex)
{
    if (mutex == NULL) {
        return VIBE_EINVAL;
    }
    mutex->owner            = NULL;
    mutex->lock_count       = 0U;
    mutex->owner_orig_prio  = 0U;
    vibe_list_init(&mutex->wait_list);
    return VIBE_OK;
}

vibe_err_t vibe_mutex_lock(vibe_mutex_t *mutex, vibe_tick_t timeout)
{
    if (mutex == NULL) {
        return VIBE_EINVAL;
    }

    vibe_thread_t *self = vibe_thread_self();

    vibe_irq_key_t key = arch_irq_lock();

    /* Recursive lock by the same owner. */
    if (mutex->owner == self) {
        mutex->lock_count++;
        arch_irq_unlock(key);
        return VIBE_OK;
    }

    /* Mutex is free — take it. */
    if (mutex->owner == NULL) {
        mutex->owner           = self;
        mutex->lock_count      = 1U;
        mutex->owner_orig_prio = self->priority;
        arch_irq_unlock(key);
        return VIBE_OK;
    }

    /* Mutex is held by another thread. */
    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return VIBE_EBUSY;
    }

    /* Priority inheritance: if we have higher priority than the owner,
     * temporarily boost the owner's priority. */
    if (self->priority > mutex->owner->priority) {
        mutex->owner->priority = self->priority;
        /* Re-enqueue the owner at its new priority if it is READY. */
        if (mutex->owner->state == VIBE_THREAD_READY) {
            _vibe_sched_dequeue(mutex->owner);
            _vibe_sched_enqueue(mutex->owner);
        }
    }

    /* Block this thread on the mutex wait list. */
    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;

    /* Insert into wait list sorted by priority (highest first). */
    vibe_list_node_t *node;
    VIBE_LIST_FOR_EACH(&mutex->wait_list, node) {
        vibe_thread_t *t = CONTAINER_OF(node, vibe_thread_t, wait_node);
        if (self->priority > t->priority) {
            vibe_list_insert_after(&mutex->wait_list,
                                   node->prev ? node->prev : NULL,
                                   &self->wait_node);
            goto inserted;
        }
    }
    vibe_list_append(&mutex->wait_list, &self->wait_node);

inserted:
    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* When we return here the mutex has been given to us (or timed out). */
    if (self->state == VIBE_THREAD_RUNNING || self->state == VIBE_THREAD_READY) {
        /* Check if we actually got the mutex. */
        if (mutex->owner == self) {
            return VIBE_OK;
        }
    }

    return VIBE_ETIMEOUT;
}

vibe_err_t vibe_mutex_trylock(vibe_mutex_t *mutex)
{
    return vibe_mutex_lock(mutex, VIBE_NO_WAIT);
}

vibe_err_t vibe_mutex_unlock(vibe_mutex_t *mutex)
{
    if (mutex == NULL) {
        return VIBE_EINVAL;
    }

    vibe_thread_t *self = vibe_thread_self();
    vibe_irq_key_t key  = arch_irq_lock();

    if (mutex->owner != self) {
        arch_irq_unlock(key);
        return VIBE_EPERM;
    }

    mutex->lock_count--;
    if (mutex->lock_count > 0U) {
        /* Still recursively locked. */
        arch_irq_unlock(key);
        return VIBE_OK;
    }

    /* Restore owner's original priority (undo PI boost). */
    self->priority = mutex->owner_orig_prio;
    /* Re-enqueue at restored priority. */
    if (self->state == VIBE_THREAD_READY) {
        _vibe_sched_dequeue(self);
        _vibe_sched_enqueue(self);
    }

    /* Wake the highest-priority waiter. */
    vibe_list_node_t *head = vibe_list_get_head(&mutex->wait_list);
    if (head != NULL) {
        vibe_thread_t *next = CONTAINER_OF(head, vibe_thread_t, wait_node);
        mutex->owner           = next;
        mutex->lock_count      = 1U;
        mutex->owner_orig_prio = next->base_priority;
        _vibe_sched_enqueue(next);
    } else {
        mutex->owner      = NULL;
        mutex->lock_count = 0U;
    }

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    return VIBE_OK;
}

bool vibe_mutex_is_owner(const vibe_mutex_t *mutex)
{
    return mutex != NULL && mutex->owner == vibe_thread_self();
}
