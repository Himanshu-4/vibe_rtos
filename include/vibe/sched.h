#ifndef VIBE_SCHED_H
#define VIBE_SCHED_H

/**
 * @file sched.h
 * @brief Scheduler public and internal API for VibeRTOS.
 *
 * Exposes the scheduler control functions (lock, unlock, mode) and the
 * internal functions used by the kernel (enqueue, dequeue, next-thread
 * selection). Internal functions are prefixed with _vibe_sched_.
 */

#include "vibe/types.h"
#include "vibe/thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Scheduler lock (preemption control)
 * --------------------------------------------------------------------- */

/**
 * @brief Lock the scheduler — prevent preemption of the current thread.
 *
 * Calls can be nested; the scheduler is re-enabled only when the lock
 * count returns to zero. Does NOT disable interrupts.
 */
void vibe_sched_lock(void);

/**
 * @brief Unlock the scheduler.
 *
 * Decrements the lock count. When it reaches zero, a context switch
 * is triggered if a higher-priority thread is ready.
 */
void vibe_sched_unlock(void);

/**
 * @brief Return the current scheduler lock depth (0 = not locked).
 */
int vibe_sched_lock_count(void);

/* -----------------------------------------------------------------------
 * Scheduler mode
 * --------------------------------------------------------------------- */

/**
 * @brief Set the global scheduler power/performance mode.
 *
 * - VIBE_SCHED_LOW_POWER:   Reduces tick rate, extends tickless sleep.
 * - VIBE_SCHED_MID:         Balanced (default).
 * - VIBE_SCHED_PERFORMANCE: Full tick rate, no low-power idle.
 *
 * @param mode  New scheduler mode.
 */
void vibe_sched_set_mode(vibe_sched_mode_t mode);

/**
 * @brief Get the current scheduler mode.
 */
vibe_sched_mode_t vibe_sched_get_mode(void);

/* -----------------------------------------------------------------------
 * Scheduler start
 * --------------------------------------------------------------------- */

/**
 * @brief Start the scheduler and enter the idle loop.
 *
 * Called once from vibe_init(). Selects the highest-priority ready thread
 * and performs the first context switch. Never returns.
 */
void vibe_sched_start(void) __attribute__((noreturn));

/* -----------------------------------------------------------------------
 * Internal scheduler API (used by kernel/thread.c and arch)
 * --------------------------------------------------------------------- */

/**
 * @brief Select the next thread to run.
 *
 * Scans the run queues from highest to lowest priority and returns the
 * first ready thread. For equal-priority threads, round-robin is applied.
 *
 * Must be called with the scheduler lock held or IRQs disabled.
 *
 * @return Pointer to the selected thread, or the idle thread if all queues empty.
 */
vibe_thread_t *_vibe_sched_next_thread(void);

/**
 * @brief Enqueue a thread into the run queues.
 *
 * Sets thread state to READY and inserts it at the tail of its priority queue.
 *
 * @param thread  Thread to enqueue.
 */
void _vibe_sched_enqueue(vibe_thread_t *thread);

/**
 * @brief Remove a thread from the run queues.
 *
 * Must be called before blocking a thread on a wait list or suspending it.
 *
 * @param thread  Thread to dequeue.
 */
void _vibe_sched_dequeue(vibe_thread_t *thread);

/**
 * @brief Trigger a context switch to the next ready thread.
 *
 * Calls arch_switch_to(current, next). If called from thread context,
 * pends a PendSV on ARM Cortex-M. May be a no-op if sched is locked.
 */
void _vibe_sched_reschedule(void);

/**
 * @brief Move thread to the sleep list with the given expiry tick.
 *
 * @param thread        Thread to sleep.
 * @param expiry_ticks  Absolute tick count when thread should wake up.
 */
void _vibe_sched_sleep(vibe_thread_t *thread, vibe_tick_t expiry_ticks);

/**
 * @brief Wake all sleeping threads whose timeout has expired.
 *
 * Called from the system tick handler.
 *
 * @param current_tick  Current tick value.
 */
void _vibe_sched_wake_expired(vibe_tick_t current_tick);

/**
 * @brief Calculate the number of ticks until the next scheduled wakeup.
 *
 * Used by the tickless idle implementation to program the hardware timer.
 *
 * @return Ticks until next wakeup, or VIBE_WAIT_FOREVER if nothing pending.
 */
vibe_tick_t _vibe_sched_next_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SCHED_H */
