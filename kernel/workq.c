/**
 * @file kernel/workq.c
 * @brief Work queue implementation for VibeRTOS.
 *
 * Work queues allow deferred execution of short tasks from ISR or
 * high-priority threads in the context of a dedicated worker thread.
 */

#include "vibe/workq.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/sem.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include <string.h>

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

/* -----------------------------------------------------------------------
 * Worker thread entry
 * --------------------------------------------------------------------- */

static void _workq_thread_entry(void *arg)
{
    vibe_workq_t *workq = (vibe_workq_t *)arg;

    for (;;) {
        /* Wait until work is available. */
        vibe_sem_take(&workq->sem, VIBE_WAIT_FOREVER);

        /* Dequeue and execute the next work item. */
        vibe_irq_key_t key = arch_irq_lock();
        vibe_work_t *work  = workq->head;
        if (work != NULL) {
            workq->head = work->next;
            if (workq->head == NULL) {
                workq->tail = NULL;
            }
            work->next    = NULL;
            work->pending = false;
        }
        arch_irq_unlock(key);

        if (work != NULL && work->handler != NULL) {
            work->handler(work);
        }
    }
}

/* -----------------------------------------------------------------------
 * vibe_workq_init()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_workq_init(vibe_workq_t *workq,
                            void         *stack,
                            size_t        stack_size,
                            uint8_t       priority,
                            const char   *name)
{
    if (workq == NULL || stack == NULL || stack_size == 0U) {
        return VIBE_EINVAL;
    }

    memset(workq, 0, sizeof(*workq));
    vibe_sem_init(&workq->sem, 0U, 0U);

    /* Allocate a thread object for the worker. */
    static vibe_thread_t _workq_thread; /* TODO: one per workq — extend for multiple queues */
    workq->thread = &_workq_thread;

    vibe_thread_attr_t attr = {
        .stack_size = stack_size,
        .priority   = priority,
        .name       = name ? name : "workq",
        .is_static  = true,
        .cpu_id     = -1,
    };

    return vibe_thread_create(workq->thread, _workq_thread_entry,
                               workq, &attr, stack);
}

/* -----------------------------------------------------------------------
 * Work item init / submit
 * --------------------------------------------------------------------- */

vibe_err_t vibe_work_init(vibe_work_t *work, vibe_work_handler_t handler)
{
    if (work == NULL || handler == NULL) {
        return VIBE_EINVAL;
    }
    work->handler = handler;
    work->next    = NULL;
    work->pending = false;
    return VIBE_OK;
}

vibe_err_t vibe_work_submit(vibe_workq_t *workq, vibe_work_t *work)
{
    if (workq == NULL || work == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (work->pending) {
        arch_irq_unlock(key);
        return VIBE_EBUSY;
    }

    work->pending = true;
    work->next    = NULL;

    if (workq->tail != NULL) {
        workq->tail->next = work;
    } else {
        workq->head = work;
    }
    workq->tail = work;

    arch_irq_unlock(key);

    /* Signal the worker. */
    vibe_sem_give(&workq->sem);
    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Delayed work
 * --------------------------------------------------------------------- */

static void _dwork_timer_cb(vibe_timer_t *timer, void *user_data)
{
    vibe_dwork_t *dwork = (vibe_dwork_t *)user_data;
    (void)timer;
    vibe_work_submit(dwork->workq, &dwork->work);
}

vibe_err_t vibe_dwork_init(vibe_dwork_t *dwork, vibe_work_handler_t handler)
{
    if (dwork == NULL) {
        return VIBE_EINVAL;
    }
    vibe_work_init(&dwork->work, handler);
    vibe_timer_init(&dwork->timer, _dwork_timer_cb, dwork, VIBE_TIMER_ONE_SHOT);
    dwork->workq = NULL;
    return VIBE_OK;
}

vibe_err_t vibe_dwork_submit(vibe_workq_t *workq,
                              vibe_dwork_t *dwork,
                              uint32_t      delay_ms)
{
    if (workq == NULL || dwork == NULL) {
        return VIBE_EINVAL;
    }
    dwork->workq = workq;
    return vibe_timer_start(&dwork->timer, delay_ms);
}

vibe_err_t vibe_dwork_cancel(vibe_dwork_t *dwork)
{
    if (dwork == NULL) {
        return VIBE_EINVAL;
    }
    if (dwork->work.pending) {
        return VIBE_EAGAIN;
    }
    return vibe_timer_stop(&dwork->timer);
}
