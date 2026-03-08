#ifndef VIBE_WORKQ_H
#define VIBE_WORKQ_H

/**
 * @file workq.h
 * @brief Work queue API for VibeRTOS.
 *
 * Work queues allow ISRs and high-priority threads to defer processing
 * to a dedicated kernel thread. Immediate and delayed (timer-based) work
 * items are both supported.
 */

#include "vibe/types.h"
#include "vibe/timer.h"
#include "vibe/sem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct vibe_thread;
struct vibe_workq;
struct vibe_work;

/* -----------------------------------------------------------------------
 * Work item
 * --------------------------------------------------------------------- */

/** Work handler function signature. */
typedef void (*vibe_work_handler_t)(struct vibe_work *work);

typedef struct vibe_work {
    vibe_work_handler_t  handler;  /**< Function to call when work is executed. */
    struct vibe_work    *next;     /**< Intrusive link in the work queue. */
    bool                 pending;  /**< True if submitted but not yet executed. */
} vibe_work_t;

/* -----------------------------------------------------------------------
 * Delayed work item
 * --------------------------------------------------------------------- */

typedef struct vibe_dwork {
    vibe_work_t   work;         /**< Embedded work item (must be first). */
    vibe_timer_t  timer;        /**< Timer used for the delay. */
    struct vibe_workq *workq;   /**< Target work queue to submit to on expiry. */
} vibe_dwork_t;

/* -----------------------------------------------------------------------
 * Work queue
 * --------------------------------------------------------------------- */

typedef struct vibe_workq {
    struct vibe_thread *thread;   /**< Worker thread. */
    vibe_work_t        *head;     /**< Head of the pending work list. */
    vibe_work_t        *tail;     /**< Tail of the pending work list. */
    vibe_sem_t          sem;      /**< Counts pending items; worker sleeps on this. */
} vibe_workq_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_WORKQ_DEFINE — statically define a work queue with its worker thread.
 *
 * @param name        C identifier.
 * @param stack_sz    Worker thread stack size in bytes.
 * @param prio        Worker thread priority.
 */
#define VIBE_WORKQ_DEFINE(name, stack_sz, prio)                   \
    static uint8_t _##name##_stack[stack_sz]                      \
        __attribute__((aligned(8)));                               \
    vibe_workq_t name /* thread set up by vibe_workq_init() */

/* -----------------------------------------------------------------------
 * System work queue (provided by kernel/workq.c)
 * --------------------------------------------------------------------- */

/** The global system work queue. Created at kernel init time. */
extern vibe_workq_t vibe_sys_workq;

/* -----------------------------------------------------------------------
 * Work queue API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise and start a work queue.
 *
 * Creates the worker thread and initialises internal state.
 *
 * @param workq       Work queue to initialise.
 * @param stack       Pre-allocated stack buffer for the worker thread.
 * @param stack_size  Size of the stack buffer.
 * @param priority    Worker thread priority.
 * @param name        Name for the worker thread (or NULL).
 * @return            VIBE_OK or negative error.
 */
vibe_err_t vibe_workq_init(vibe_workq_t *workq,
                            void         *stack,
                            size_t        stack_size,
                            uint8_t       priority,
                            const char   *name);

/**
 * @brief Initialise a work item.
 *
 * @param work     Work item to initialise.
 * @param handler  Handler function.
 * @return         VIBE_OK.
 */
vibe_err_t vibe_work_init(vibe_work_t *work, vibe_work_handler_t handler);

/**
 * @brief Submit a work item to a work queue.
 *
 * Safe to call from ISR and thread context. If work is already pending,
 * this is a no-op (it won't be submitted twice).
 *
 * @param workq  Target work queue.
 * @param work   Work item to submit.
 * @return       VIBE_OK or VIBE_EBUSY (already pending).
 */
vibe_err_t vibe_work_submit(vibe_workq_t *workq, vibe_work_t *work);

/**
 * @brief Initialise a delayed work item.
 *
 * @param dwork    Delayed work item.
 * @param handler  Handler function.
 * @return         VIBE_OK.
 */
vibe_err_t vibe_dwork_init(vibe_dwork_t *dwork, vibe_work_handler_t handler);

/**
 * @brief Submit a work item with a delay.
 *
 * Starts an internal timer. When the timer expires the work item is
 * submitted to the specified work queue.
 *
 * @param workq     Target work queue.
 * @param dwork     Delayed work item.
 * @param delay_ms  Delay before submission in milliseconds.
 * @return          VIBE_OK.
 */
vibe_err_t vibe_dwork_submit(vibe_workq_t *workq,
                              vibe_dwork_t *dwork,
                              uint32_t      delay_ms);

/**
 * @brief Cancel a pending delayed work item.
 *
 * Stops the associated timer. If the work has already been submitted to
 * the queue, it cannot be cancelled.
 *
 * @param dwork  Delayed work item to cancel.
 * @return       VIBE_OK if cancelled, VIBE_EAGAIN if already executing.
 */
vibe_err_t vibe_dwork_cancel(vibe_dwork_t *dwork);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_WORKQ_H */
