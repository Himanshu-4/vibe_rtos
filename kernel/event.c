/**
 * @file kernel/event.c
 * @brief Event flags / event group implementation for VibeRTOS.
 */

#include "vibe/event.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/list.h"
#include <string.h>

#include "vibe/arch.h"

/* Per-thread event wait context stored inline using the TLS field. */
typedef struct {
    uint32_t               wait_flags;
    vibe_event_wait_mode_t wait_mode;
    bool                   satisfied;
} _event_wait_ctx_t;

vibe_err_t vibe_event_init(vibe_event_t *event)
{
    if (event == NULL) {
        return VIBE_EINVAL;
    }
    event->flags = 0U;
    vibe_list_init(&event->wait_list);
    return VIBE_OK;
}

static bool _flags_satisfied(uint32_t current, uint32_t requested,
                               vibe_event_wait_mode_t mode)
{
    if (mode == VIBE_EVENT_WAIT_ALL) {
        return (current & requested) == requested;
    } else {
        return (current & requested) != 0U;
    }
}

vibe_err_t vibe_event_post(vibe_event_t *event, uint32_t flags)
{
    if (event == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    event->flags |= flags;

    /* Wake all waiters whose conditions are now satisfied. */
    vibe_list_node_t *node, *tmp;
    VIBE_LIST_FOR_EACH_SAFE(&event->wait_list, node, tmp) {
        vibe_thread_t *t = CONTAINER_OF(node, vibe_thread_t, wait_node);
        /* Recover wait context — stored in tls for simplicity here.
         * A production implementation would embed a proper ctx struct. */
        _event_wait_ctx_t *ctx = (_event_wait_ctx_t *)t->tls;
        if (ctx != NULL && _flags_satisfied(event->flags,
                                              ctx->wait_flags, ctx->wait_mode)) {
            ctx->satisfied = true;
            vibe_list_remove(&event->wait_list, node);
            _vibe_sched_enqueue(t);
        }
    }

    arch_irq_unlock(key);
    _vibe_sched_reschedule();
    return VIBE_OK;
}

vibe_err_t vibe_event_clear(vibe_event_t *event, uint32_t flags)
{
    if (event == NULL) {
        return VIBE_EINVAL;
    }
    vibe_irq_key_t key = arch_irq_lock();
    event->flags &= ~flags;
    arch_irq_unlock(key);
    return VIBE_OK;
}

vibe_err_t vibe_event_wait(vibe_event_t          *event,
                            uint32_t               flags,
                            vibe_event_wait_mode_t mode,
                            vibe_tick_t            timeout)
{
    if (event == NULL || flags == 0U) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    /* Check immediately. */
    if (_flags_satisfied(event->flags, flags, mode)) {
        arch_irq_unlock(key);
        return VIBE_OK;
    }

    if (timeout == VIBE_NO_WAIT) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }

    /* Block. */
    vibe_thread_t     *self = vibe_thread_self();
    _event_wait_ctx_t *ctx  = (_event_wait_ctx_t *)self->tls;
    if (ctx != NULL) {
        ctx->wait_flags = flags;
        ctx->wait_mode  = mode;
        ctx->satisfied  = false;
    }

    _vibe_sched_dequeue(self);
    self->state   = VIBE_THREAD_WAITING;
    self->timeout = (timeout == VIBE_WAIT_FOREVER) ? VIBE_WAIT_FOREVER
                    : vibe_tick_get() + timeout;
    vibe_list_append(&event->wait_list, &self->wait_node);

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    /* Check outcome. */
    if (ctx != NULL && ctx->satisfied) {
        return VIBE_OK;
    }
    return VIBE_ETIMEOUT;
}

uint32_t vibe_event_get(const vibe_event_t *event)
{
    return event ? event->flags : 0U;
}
