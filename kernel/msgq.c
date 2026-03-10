/**
 * @file kernel/msgq.c
 * @brief Fixed-size message queue (ring buffer FIFO) for VibeRTOS.
 */

#include "vibe/msgq.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/list.h"
#include <string.h>

#include "vibe/arch.h"

vibe_err_t vibe_msgq_init(vibe_msgq_t *q,
                           void        *buffer,
                           size_t       msg_size,
                           uint32_t     max_msgs)
{
    if (q == NULL || buffer == NULL || msg_size == 0U || max_msgs == 0U) {
        return VIBE_EINVAL;
    }
    q->buffer   = (uint8_t *)buffer;
    q->msg_size = msg_size;
    q->max_msgs = max_msgs;
    q->head     = 0U;
    q->tail     = 0U;
    q->used     = 0U;
    vibe_list_init(&q->read_wait);
    vibe_list_init(&q->write_wait);
    return VIBE_OK;
}

vibe_err_t vibe_msgq_put(vibe_msgq_t *q, const void *data, vibe_tick_t timeout)
{
    if (q == NULL || data == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (q->used < q->max_msgs) {
        /* Space available — copy message into ring buffer. */
        memcpy(q->buffer + q->tail * q->msg_size, data, q->msg_size);
        q->tail = (q->tail + 1U) % q->max_msgs;
        q->used++;

        /* Wake a blocked reader. */
        vibe_list_node_t *rn = vibe_list_get_head(&q->read_wait);
        if (rn != NULL) {
            vibe_thread_t *reader = CONTAINER_OF(rn, vibe_thread_t, wait_node);
            _vibe_sched_enqueue(reader);
        }

        arch_irq_unlock(key);
        _vibe_sched_reschedule();
        return VIBE_OK;
    }

    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }

    /* Queue full — block. */
    vibe_thread_t *self = vibe_thread_self();
    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;
    vibe_list_append(&q->write_wait, &self->wait_node);

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* After wakeup, try again (space should be free now). */
    key = arch_irq_lock();
    if (q->used < q->max_msgs) {
        memcpy(q->buffer + q->tail * q->msg_size, data, q->msg_size);
        q->tail = (q->tail + 1U) % q->max_msgs;
        q->used++;
        arch_irq_unlock(key);
        return VIBE_OK;
    }
    arch_irq_unlock(key);
    return VIBE_ETIMEOUT;
}

vibe_err_t vibe_msgq_get(vibe_msgq_t *q, void *buf, vibe_tick_t timeout)
{
    if (q == NULL || buf == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (q->used > 0U) {
        memcpy(buf, q->buffer + q->head * q->msg_size, q->msg_size);
        q->head = (q->head + 1U) % q->max_msgs;
        q->used--;

        /* Wake a blocked writer. */
        vibe_list_node_t *wn = vibe_list_get_head(&q->write_wait);
        if (wn != NULL) {
            vibe_thread_t *writer = CONTAINER_OF(wn, vibe_thread_t, wait_node);
            _vibe_sched_enqueue(writer);
        }

        arch_irq_unlock(key);
        _vibe_sched_reschedule();
        return VIBE_OK;
    }

    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }

    /* Queue empty — block. */
    vibe_thread_t *self = vibe_thread_self();
    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;
    vibe_list_append(&q->read_wait, &self->wait_node);

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* After wakeup, try again. */
    key = arch_irq_lock();
    if (q->used > 0U) {
        memcpy(buf, q->buffer + q->head * q->msg_size, q->msg_size);
        q->head = (q->head + 1U) % q->max_msgs;
        q->used--;
        arch_irq_unlock(key);
        return VIBE_OK;
    }
    arch_irq_unlock(key);
    return VIBE_ETIMEOUT;
}

vibe_err_t vibe_msgq_peek(const vibe_msgq_t *q, void *buf)
{
    if (q == NULL || buf == NULL) {
        return VIBE_EINVAL;
    }
    vibe_irq_key_t key = arch_irq_lock();
    if (q->used == 0U) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }
    memcpy(buf, q->buffer + q->head * q->msg_size, q->msg_size);
    arch_irq_unlock(key);
    return VIBE_OK;
}

void vibe_msgq_purge(vibe_msgq_t *q)
{
    if (q == NULL) {
        return;
    }
    vibe_irq_key_t key = arch_irq_lock();
    q->head = 0U;
    q->tail = 0U;
    q->used = 0U;

    /* Wake all blocked writers since there is now space. */
    vibe_list_node_t *wn;
    while ((wn = vibe_list_get_head(&q->write_wait)) != NULL) {
        vibe_thread_t *t = CONTAINER_OF(wn, vibe_thread_t, wait_node);
        _vibe_sched_enqueue(t);
    }
    arch_irq_unlock(key);
    _vibe_sched_reschedule();
}

uint32_t vibe_msgq_count(const vibe_msgq_t *q)
{
    return q ? q->used : 0U;
}
