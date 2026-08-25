#ifndef VIBE_TRACE_H
#define VIBE_TRACE_H

/**
 * @file trace.h
 * @brief Kernel tracing hooks and per-thread runtime statistics.
 *
 * When CONFIG_TRACE is enabled, the kernel calls a set of trace hook
 * functions at every significant scheduling event (thread switch, thread
 * lifecycle, ISR entry/exit, system tick). The hooks are defined as WEAK
 * no-ops in kernel/trace.c — a trace library (SEGGER SystemView,
 * Percepio Tracealyzer, or a custom recorder streaming over RTT) can
 * override any of them with strong definitions without touching kernel
 * code.
 *
 * When CONFIG_TRACE is disabled every VIBE_TRACE_* macro compiles to
 * nothing, so instrumented kernel code carries zero overhead.
 *
 * CONFIG_TRACE also adds runtime-statistics fields to each TCB
 * (see vibe_thread_t in thread.h): number of times scheduled in, tick of
 * the last switch-in, and accumulated run time in ticks. These are
 * maintained by the scheduler and can be read with
 * vibe_trace_thread_stats().
 */

#include <stdint.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration — avoids a circular include with thread.h. */
struct vibe_thread;

#ifdef CONFIG_TRACE

/* -----------------------------------------------------------------------
 * Trace hook functions — WEAK no-ops in kernel/trace.c.
 * Override with strong definitions from your trace library.
 * --------------------------------------------------------------------- */

/** A thread was created and entered the ready queue for the first time. */
void vibe_trace_thread_create(struct vibe_thread *thread);

/** A thread was deleted / its entry function returned. */
void vibe_trace_thread_exit(struct vibe_thread *thread);

/** A thread became ready to run (enqueued on a run queue). */
void vibe_trace_thread_ready(struct vibe_thread *thread);

/** The scheduler switched this thread out (it stops executing). */
void vibe_trace_thread_switch_out(struct vibe_thread *thread);

/** The scheduler switched this thread in (it starts executing). */
void vibe_trace_thread_switch_in(struct vibe_thread *thread);

/** A thread entered a timed sleep until @p wake_tick. */
void vibe_trace_thread_sleep(struct vibe_thread *thread,
                             vibe_tick_t wake_tick);

/** Interrupt service routine entered. */
void vibe_trace_isr_enter(void);

/** Interrupt service routine exited. */
void vibe_trace_isr_exit(void);

/** System tick advanced to @p tick. */
void vibe_trace_tick(vibe_tick_t tick);

/** The CPU is about to enter its low-power idle state (WFI). */
void vibe_trace_idle(void);

/* -----------------------------------------------------------------------
 * Instrumentation macros — used by kernel code
 * --------------------------------------------------------------------- */

#define VIBE_TRACE_THREAD_CREATE(t)      vibe_trace_thread_create(t)
#define VIBE_TRACE_THREAD_EXIT(t)        vibe_trace_thread_exit(t)
#define VIBE_TRACE_THREAD_READY(t)       vibe_trace_thread_ready(t)
#define VIBE_TRACE_THREAD_SWITCH_OUT(t)  vibe_trace_thread_switch_out(t)
#define VIBE_TRACE_THREAD_SWITCH_IN(t)   vibe_trace_thread_switch_in(t)
#define VIBE_TRACE_THREAD_SLEEP(t, w)    vibe_trace_thread_sleep((t), (w))
#define VIBE_TRACE_ISR_ENTER()           vibe_trace_isr_enter()
#define VIBE_TRACE_ISR_EXIT()            vibe_trace_isr_exit()
#define VIBE_TRACE_TICK(n)               vibe_trace_tick(n)
#define VIBE_TRACE_IDLE()                vibe_trace_idle()

#else /* !CONFIG_TRACE — all hooks compile to nothing */

#define VIBE_TRACE_THREAD_CREATE(t)      ((void)0)
#define VIBE_TRACE_THREAD_EXIT(t)        ((void)0)
#define VIBE_TRACE_THREAD_READY(t)       ((void)0)
#define VIBE_TRACE_THREAD_SWITCH_OUT(t)  ((void)0)
#define VIBE_TRACE_THREAD_SWITCH_IN(t)   ((void)0)
#define VIBE_TRACE_THREAD_SLEEP(t, w)    ((void)0)
#define VIBE_TRACE_ISR_ENTER()           ((void)0)
#define VIBE_TRACE_ISR_EXIT()            ((void)0)
#define VIBE_TRACE_TICK(n)               ((void)0)
#define VIBE_TRACE_IDLE()                ((void)0)

#endif /* CONFIG_TRACE */

/* -----------------------------------------------------------------------
 * Per-thread runtime statistics accessor
 * --------------------------------------------------------------------- */

/**
 * Snapshot of a thread's scheduling statistics.
 * Only populated when CONFIG_TRACE is enabled; zeroed otherwise.
 */
typedef struct {
    uint32_t    switch_in_count;  /**< Times the thread was scheduled in. */
    vibe_tick_t last_switch_in;   /**< Tick of the most recent switch-in. */
    vibe_tick_t total_runtime;    /**< Accumulated run time in ticks. */
} vibe_trace_thread_stats_t;

/**
 * @brief Read a thread's runtime statistics.
 *
 * @param thread  Thread to query (NULL = current thread).
 * @param stats   Output. Zeroed when tracing is disabled.
 * @return        VIBE_OK, or VIBE_EINVAL if stats is NULL or no thread.
 */
vibe_err_t vibe_trace_thread_stats(struct vibe_thread *thread,
                                   vibe_trace_thread_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_TRACE_H */
