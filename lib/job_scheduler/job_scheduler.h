/**
 * @file lib/job_scheduler/job_scheduler.h
 * @brief Cooperative job scheduler for VibeRTOS.
 *
 * The job scheduler runs multiple async coroutines cooperatively inside
 * a single RTOS task.  Each "job" wraps one coroutine function and its
 * context.  On every call to vibe_sched_run(), the scheduler:
 *
 *   Phase 1 — runs all READY jobs once, in descending priority order.
 *   Phase 2 — re-invokes all BLOCKED jobs so they can re-evaluate their
 *              ASYNC_AWAIT conditions.
 *
 * Jobs that return ASYNC_DONE are freed back to the pool automatically.
 *
 * ---------------------------------------------------------------------------
 * TYPICAL USAGE
 * ---------------------------------------------------------------------------
 *
 *   // 1. Declare a scheduler instance (typically in task state or as static):
 *   static vibe_job_sched_t g_sched;
 *
 *   // 2. Define a coroutine:
 *   async_status_t blink(void *ctx, async_t *co) {
 *       uint32_t *ticks = (uint32_t *)ctx;
 *       ASYNC_BEGIN(co);
 *       while (1) {
 *           gpio_toggle(LED_PIN);
 *           *ticks = vibe_tick_get() + MS_TO_TICKS(500);
 *           ASYNC_AWAIT(co, vibe_tick_get() >= *ticks);
 *       }
 *       ASYNC_END(co);
 *   }
 *
 *   // 3. In the RTOS task entry:
 *   void led_task(void *arg) {
 *       static uint32_t deadline;
 *       vibe_sched_init(&g_sched);
 *       vibe_sched_submit(&g_sched, blink, &deadline, 0);
 *
 *       while (1) {
 *           vibe_sched_run(&g_sched);
 *           if (vibe_sched_is_idle(&g_sched))
 *               vibe_thread_yield();   // give other RTOS tasks a chance
 *       }
 *   }
 *
 * ---------------------------------------------------------------------------
 * CONCURRENCY MODEL
 * ---------------------------------------------------------------------------
 *
 * The scheduler is single-threaded within one RTOS task.  Multiple tasks
 * can each own a separate vibe_job_sched_t with no locking required.
 * Cross-task communication must use RTOS primitives (semaphores, queues).
 *
 * @note CONFIG_JOB_SCHEDULER and CONFIG_ASYNC must be enabled.
 */

#ifndef VIBE_JOB_SCHEDULER_H
#define VIBE_JOB_SCHEDULER_H

#include "async.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Compile-time capacity (overridden by Kconfig / autoconf.h)
 * ---------------------------------------------------------------------- */

#ifndef CONFIG_JOB_SCHEDULER_MAX_JOBS
#define CONFIG_JOB_SCHEDULER_MAX_JOBS 8
#endif

/* -------------------------------------------------------------------------
 * Job state
 * ---------------------------------------------------------------------- */

/**
 * Lifecycle state of a single job slot.
 */
typedef enum {
    JOB_IDLE    = 0,  /**< Slot is free — may be reused.                   */
    JOB_READY   = 1,  /**< Coroutine wants to run; scheduled next tick.    */
    JOB_BLOCKED = 2,  /**< Waiting for a condition; probed every tick.     */
} job_state_t;

/* -------------------------------------------------------------------------
 * Job descriptor
 * ---------------------------------------------------------------------- */

/**
 * One slot in the scheduler's static job pool.
 *
 * Do not access fields directly; use the vibe_sched_* API.
 */
typedef struct vibe_job {
    async_fn_t  fn;       /**< Coroutine function to invoke.                */
    void       *ctx;      /**< User context pointer passed to @p fn.        */
    async_t     co;       /**< Coroutine resume-point state.                */
    job_state_t state;    /**< Current scheduling state.                    */
    uint8_t     prio;     /**< Priority: 255 = highest, 0 = lowest.        */
#if defined(CONFIG_JOB_SCHEDULER_STATS)
    uint32_t    run_count; /**< How many times this job has been resumed.   */
#endif
} vibe_job_t;

/* -------------------------------------------------------------------------
 * Scheduler statistics (optional)
 * ---------------------------------------------------------------------- */

#if defined(CONFIG_JOB_SCHEDULER_STATS)
/**
 * Aggregate statistics for a vibe_job_sched_t instance.
 * Retrieved via vibe_sched_get_stats().
 */
typedef struct {
    uint32_t submitted;  /**< Total jobs ever submitted to this scheduler. */
    uint32_t completed;  /**< Total jobs that returned ASYNC_DONE.         */
    uint32_t active;     /**< Currently occupied (non-idle) job slots.     */
    uint32_t ticks;      /**< Total vibe_sched_run() calls.                */
} vibe_sched_stats_t;
#endif

/* -------------------------------------------------------------------------
 * Scheduler instance
 * ---------------------------------------------------------------------- */

/**
 * Cooperative job scheduler.
 *
 * Declare as static or embed in a task struct.  Initialise once with
 * vibe_sched_init() before submitting any jobs.
 */
typedef struct {
    vibe_job_t jobs[CONFIG_JOB_SCHEDULER_MAX_JOBS]; /**< Fixed job pool.   */
    uint8_t    capacity; /**< Always == CONFIG_JOB_SCHEDULER_MAX_JOBS.     */
#if defined(CONFIG_JOB_SCHEDULER_STATS)
    vibe_sched_stats_t stats;
#endif
} vibe_job_sched_t;

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialise a job scheduler instance.
 *
 * Must be called exactly once before any other vibe_sched_* function.
 *
 * @param sched  Scheduler to initialise.
 */
void vibe_sched_init(vibe_job_sched_t *sched);

/**
 * @brief Submit a new job (coroutine) to the scheduler.
 *
 * The job is placed in a free slot and marked READY.  It will be executed
 * on the next vibe_sched_run() call.
 *
 * @param sched  Scheduler instance.
 * @param fn     Coroutine function (must match async_fn_t).
 * @param ctx    Context pointer passed to @p fn on every invocation.
 * @param prio   Scheduling priority (255 = highest, 0 = lowest).
 *               Higher-priority jobs run before lower-priority ones within
 *               the same scheduler tick.
 * @return  Pointer to the allocated job slot, or NULL if the pool is full.
 */
vibe_job_t *vibe_sched_submit(vibe_job_sched_t *sched,
                               async_fn_t fn, void *ctx, uint8_t prio);

/**
 * @brief Convenience wrapper: submit with priority 0.
 */
static inline vibe_job_t *vibe_sched_defer(vibe_job_sched_t *sched,
                                            async_fn_t fn, void *ctx)
{
    return vibe_sched_submit(sched, fn, ctx, 0U);
}

/**
 * @brief Cancel and release a running job.
 *
 * The job's slot is freed immediately.  If the coroutine holds resources,
 * clean them up before calling this.
 *
 * @param sched  Scheduler instance (used for stats update).
 * @param job    Job to cancel.  Must have been returned by vibe_sched_submit().
 */
void vibe_sched_cancel(vibe_job_sched_t *sched, vibe_job_t *job);

/**
 * @brief Run one tick of the scheduler.
 *
 * Executes every READY job once (in priority order) then re-probes every
 * BLOCKED job.  Completed jobs are automatically returned to the free pool.
 *
 * Call from the main loop of a RTOS task:
 *
 *   while (1) {
 *       vibe_sched_run(&sched);
 *       if (vibe_sched_is_idle(&sched))
 *           vibe_thread_yield();
 *   }
 *
 * @param sched  Scheduler instance.
 * @return  Number of jobs that were invoked during this tick.
 */
int vibe_sched_run(vibe_job_sched_t *sched);

/**
 * @brief Return true when no jobs are READY (all are BLOCKED or IDLE).
 *
 * Useful to decide whether to yield the RTOS task back to the kernel.
 *
 * @param sched  Scheduler instance.
 */
bool vibe_sched_is_idle(const vibe_job_sched_t *sched);

/**
 * @brief Return the number of active (non-IDLE) job slots.
 *
 * @param sched  Scheduler instance.
 */
int vibe_sched_active_count(const vibe_job_sched_t *sched);

#if defined(CONFIG_JOB_SCHEDULER_STATS)
/**
 * @brief Copy the current statistics into @p s.
 *
 * @param sched  Scheduler instance.
 * @param s      Output buffer (must not be NULL).
 */
void vibe_sched_get_stats(const vibe_job_sched_t *sched,
                           vibe_sched_stats_t *s);
#endif /* CONFIG_JOB_SCHEDULER_STATS */

#ifdef __cplusplus
}
#endif

#endif /* VIBE_JOB_SCHEDULER_H */
