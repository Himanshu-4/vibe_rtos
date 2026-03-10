/**
 * @file kernel/pipe.c
 * @brief Byte-stream pipe backed by a ring buffer for VibeRTOS.
 */

#include "vibe/pipe.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include <string.h>

#include "vibe/arch.h"

vibe_err_t vibe_pipe_init(vibe_pipe_t *pipe, void *buf, size_t size)
{
    if (pipe == NULL || buf == NULL || size == 0U) {
        return VIBE_EINVAL;
    }
    pipe->buf  = (uint8_t *)buf;
    pipe->size = size;
    pipe->head = 0U;
    pipe->tail = 0U;
    pipe->used = 0U;
    vibe_list_init(&pipe->read_wait);
    vibe_list_init(&pipe->write_wait);
    return VIBE_OK;
}

int vibe_pipe_write(vibe_pipe_t *pipe,
                    const void  *data,
                    size_t       len,
                    vibe_tick_t  timeout)
{
    if (pipe == NULL || data == NULL) {
        return (int)VIBE_EINVAL;
    }

    const uint8_t *src     = (const uint8_t *)data;
    size_t         written  = 0U;
    vibe_tick_t    deadline = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                              : vibe_tick_get() + timeout;

    while (written < len) {
        vibe_irq_key_t key = arch_irq_lock();
        size_t space = pipe->size - pipe->used;

        if (space > 0U) {
            size_t to_write = (len - written) < space ? (len - written) : space;
            for (size_t i = 0; i < to_write; i++) {
                pipe->buf[pipe->tail] = src[written + i];
                pipe->tail = (pipe->tail + 1U) % pipe->size;
            }
            pipe->used += to_write;
            written    += to_write;

            /* Wake a reader. */
            vibe_list_node_t *rn = vibe_list_get_head(&pipe->read_wait);
            if (rn != NULL) {
                vibe_thread_t *t = CONTAINER_OF(rn, vibe_thread_t, wait_node);
                _vibe_sched_enqueue(t);
            }
            arch_irq_unlock(key);
            _vibe_sched_reschedule();
        } else {
            /* No space. */
            if (timeout == VIBE_NO_WAIT) {
                arch_irq_unlock(key);
                break;
            }
            if (deadline != VIBE_WAIT_FOREVER && vibe_tick_get() >= deadline) {
                arch_irq_unlock(key);
                break;
            }

            /* Block until a reader frees space. */
            vibe_thread_t *self = vibe_thread_self();
            _vibe_sched_dequeue(self);
            self->state   = VIBE_THREAD_WAITING;
            self->timeout = deadline;
            vibe_list_append(&pipe->write_wait, &self->wait_node);
            arch_irq_unlock(key);
            _vibe_sched_reschedule();
        }
    }

    return (int)written;
}

int vibe_pipe_read(vibe_pipe_t *pipe,
                   void        *buf,
                   size_t       len,
                   vibe_tick_t  timeout)
{
    if (pipe == NULL || buf == NULL) {
        return (int)VIBE_EINVAL;
    }

    uint8_t    *dst      = (uint8_t *)buf;
    size_t      read_cnt = 0U;
    vibe_tick_t deadline = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                           : vibe_tick_get() + timeout;

    while (read_cnt < len) {
        vibe_irq_key_t key = arch_irq_lock();

        if (pipe->used > 0U) {
            size_t avail    = pipe->used;
            size_t to_read  = (len - read_cnt) < avail ? (len - read_cnt) : avail;
            for (size_t i = 0; i < to_read; i++) {
                dst[read_cnt + i] = pipe->buf[pipe->head];
                pipe->head = (pipe->head + 1U) % pipe->size;
            }
            pipe->used -= to_read;
            read_cnt   += to_read;

            /* Wake a writer. */
            vibe_list_node_t *wn = vibe_list_get_head(&pipe->write_wait);
            if (wn != NULL) {
                vibe_thread_t *t = CONTAINER_OF(wn, vibe_thread_t, wait_node);
                _vibe_sched_enqueue(t);
            }
            arch_irq_unlock(key);
            _vibe_sched_reschedule();
        } else {
            if (timeout == VIBE_NO_WAIT || read_cnt > 0U) {
                arch_irq_unlock(key);
                break;
            }
            if (deadline != VIBE_WAIT_FOREVER && vibe_tick_get() >= deadline) {
                arch_irq_unlock(key);
                break;
            }

            vibe_thread_t *self = vibe_thread_self();
            _vibe_sched_dequeue(self);
            self->state   = VIBE_THREAD_WAITING;
            self->timeout = deadline;
            vibe_list_append(&pipe->read_wait, &self->wait_node);
            arch_irq_unlock(key);
            _vibe_sched_reschedule();
        }
    }

    return (int)read_cnt;
}

size_t vibe_pipe_available_read(const vibe_pipe_t *pipe)
{
    return pipe ? pipe->used : 0U;
}

size_t vibe_pipe_available_write(const vibe_pipe_t *pipe)
{
    return pipe ? (pipe->size - pipe->used) : 0U;
}
