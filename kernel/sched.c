/**
 * @file kernel/sched.c
 * @brief VibeRTOS preemptive priority-based scheduler.
 *
 * Implements:
 *  - Per-priority FIFO run queues (round-robin at same priority)
 *  - Sleep list for time-delayed threads
 *  - Scheduler lock (preemption suppression)
 *  - Scheduler mode (LOW_POWER / MID / PERFORMANCE)
 *  - Tickless idle: computes next wakeup tick for arch_set_next_tick()
 */

#include "vibe/sched.h"
#include "vibe/thread.h"
#include "vibe/types.h"
#include "vibe/spinlock.h"
#include "vibe/trace.h"
#include "vibe/sys/list.h"
#include "vibe/sys/printk.h"
#include <string.h>

#include "vibe/arch.h"

/* -----------------------------------------------------------------------
 * Run queues — one list per priority level
 * --------------------------------------------------------------------- */

#ifndef CONFIG_NUM_PRIORITIES
#define CONFIG_NUM_PRIORITIES  32
#endif

static vibe_list_t g_run_queues[CONFIG_NUM_PRIORITIES];

/* Bitmask of non-empty priority levels (bit N set = queue[N] not empty). */
static uint32_t    g_ready_mask = 0U;

/* -----------------------------------------------------------------------
 * Sleep list — sorted by expiry tick (ascending)
 * --------------------------------------------------------------------- */

vibe_list_t g_sleep_list;

/* -----------------------------------------------------------------------
 * Current running thread (per-CPU; single CPU for now)
 * --------------------------------------------------------------------- */

static vibe_thread_t *g_current_thread = NULL;

/* Idle thread pointer — set by kernel/idle.c during init */
vibe_thread_t *g_idle_thread = NULL;

/* -----------------------------------------------------------------------
 * Scheduler state
 * --------------------------------------------------------------------- */

static int              g_sched_lock_count = 0;
static vibe_sched_mode_t g_sched_mode       = VIBE_SCHED_MID;
static vibe_spinlock_t   g_sched_lock __attribute__((unused)) = VIBE_SPINLOCK_INIT;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static inline int _highest_ready_priority(void)
{
    /* Find the highest set bit in g_ready_mask.
     * Priority VIBE_PRIO_MAX (31) is the most urgent. */
    if (g_ready_mask == 0U) {
        return -1;
    }
    /* __builtin_clz counts leading zeros; highest bit = 31 - clz. */
    return 31 - __builtin_clz(g_ready_mask);
}

/* -----------------------------------------------------------------------
 * Enqueue / dequeue
 * --------------------------------------------------------------------- */

void _vibe_sched_enqueue(vibe_thread_t *thread)
{
    vibe_irq_key_t key = arch_irq_lock();

    thread->state = VIBE_THREAD_READY;
    int prio = thread->priority;

    vibe_list_append(&g_run_queues[prio], &thread->sched_node);
    g_ready_mask |= BIT(prio);

    arch_irq_unlock(key);

    VIBE_TRACE_THREAD_READY(thread);
}

void _vibe_sched_dequeue(vibe_thread_t *thread)
{
    vibe_irq_key_t key = arch_irq_lock();

    int prio = thread->priority;
    vibe_list_remove(&g_run_queues[prio], &thread->sched_node);

    if (vibe_list_is_empty(&g_run_queues[prio])) {
        g_ready_mask &= ~BIT(prio);
    }

    arch_irq_unlock(key);
}

/* -----------------------------------------------------------------------
 * Select next thread
 * --------------------------------------------------------------------- */

vibe_thread_t *_vibe_sched_next_thread(void)
{
    int prio = _highest_ready_priority();
    if (prio < 0) {
        /* No ready threads — return idle. */
        return g_idle_thread;
    }

    vibe_list_node_t *node = vibe_list_peek_head(&g_run_queues[prio]);
    if (node == NULL) {
        return g_idle_thread;
    }

    return CONTAINER_OF(node, vibe_thread_t, sched_node);
}

/* -----------------------------------------------------------------------
 * Reschedule — trigger context switch if a better thread is available
 * --------------------------------------------------------------------- */

void _vibe_sched_reschedule(void)
{
    /* Runs from both thread and ISR context — the whole decision must be
     * atomic or a tick ISR could race a thread-context reschedule. */
    vibe_irq_key_t key = arch_irq_lock();

    if (g_sched_lock_count > 0) {
        arch_irq_unlock(key);
        return; /* Scheduler locked — don't preempt. */
    }

    vibe_thread_t *next = _vibe_sched_next_thread();
    if (next == NULL || next == g_current_thread) {
        arch_irq_unlock(key);
        return; /* Nothing to run, or already running the best thread. */
    }

    /* On Cortex-M: pend PendSV which does the actual context switch. */
    vibe_thread_t *prev = g_current_thread;
    g_current_thread = next;
    next->state = VIBE_THREAD_RUNNING;

    /* Move prev back to READY if it was running (not blocked/sleeping). */
    if (prev != NULL && prev->state == VIBE_THREAD_RUNNING) {
        prev->state = VIBE_THREAD_READY;
    }

#ifdef CONFIG_TRACE
    /* Runtime statistics for trace viewers. */
    vibe_tick_t now = vibe_tick_get();
    if (prev != NULL) {
        prev->total_runtime += now - prev->last_switch_in;
    }
    next->switch_in_count++;
    next->last_switch_in = now;
#endif
    if (prev != NULL) {
        VIBE_TRACE_THREAD_SWITCH_OUT(prev);
    }
    VIBE_TRACE_THREAD_SWITCH_IN(next);

    arch_switch_to(prev, next);

    arch_irq_unlock(key);
}

/* -----------------------------------------------------------------------
 * Round-robin time-slicing — called from the system tick handler
 * --------------------------------------------------------------------- */

void _vibe_sched_timeslice_tick(void)
{
#if defined(CONFIG_TIMESLICE_MS) && (CONFIG_TIMESLICE_MS > 0)
    static uint32_t slice_left = VIBE_MS_TO_TICKS(CONFIG_TIMESLICE_MS);

    vibe_thread_t *cur = g_current_thread;
    if (cur == NULL || cur->state != VIBE_THREAD_RUNNING) {
        return;
    }

    if (slice_left > 1U) {
        slice_left--;
        return;
    }
    slice_left = VIBE_MS_TO_TICKS(CONFIG_TIMESLICE_MS);

    /* Rotate only when another thread is ready at the same priority. */
    if (vibe_list_count(&g_run_queues[cur->priority]) > 1U) {
        _vibe_sched_dequeue(cur);
        _vibe_sched_enqueue(cur);
        /* The caller (_vibe_sys_tick_handler) reschedules right after. */
    }
#endif
}

/* -----------------------------------------------------------------------
 * Sleep list management
 * --------------------------------------------------------------------- */

void _vibe_sched_sleep(vibe_thread_t *thread, vibe_tick_t expiry_ticks)
{
    vibe_irq_key_t key = arch_irq_lock();

    thread->timeout = expiry_ticks;
    thread->state   = VIBE_THREAD_SLEEPING;

    /* Insert into sleep list sorted by expiry (ascending). */
    vibe_list_node_t *node;
    VIBE_LIST_FOR_EACH(&g_sleep_list, node) {
        vibe_thread_t *t = CONTAINER_OF(node, vibe_thread_t, sleep_node);
        if (expiry_ticks < t->timeout) {
            if (node->prev != NULL) {
                vibe_list_insert_after(&g_sleep_list, node->prev,
                                       &thread->sleep_node);
            } else {
                /* Inserting before the head — insert_after(NULL) would
                 * dereference NULL; use prepend instead. */
                vibe_list_prepend(&g_sleep_list, &thread->sleep_node);
            }
            goto done;
        }
    }
    vibe_list_append(&g_sleep_list, &thread->sleep_node);

done:
    arch_irq_unlock(key);
}

void _vibe_sched_wake_expired(vibe_tick_t current_tick)
{
    vibe_list_node_t *node, *tmp;
    VIBE_LIST_FOR_EACH_SAFE(&g_sleep_list, node, tmp) {
        vibe_thread_t *t = CONTAINER_OF(node, vibe_thread_t, sleep_node);
        if (t->timeout <= current_tick) {
            vibe_list_remove(&g_sleep_list, node);
            _vibe_sched_enqueue(t);
        } else {
            break; /* List is sorted — no need to check further. */
        }
    }
}

vibe_tick_t _vibe_sched_next_wakeup(void)
{
    vibe_list_node_t *head = vibe_list_peek_head(&g_sleep_list);
    if (head == NULL) {
        return VIBE_WAIT_FOREVER;
    }
    vibe_thread_t *t = CONTAINER_OF(head, vibe_thread_t, sleep_node);
    return t->timeout;
}

/* -----------------------------------------------------------------------
 * Scheduler lock / unlock
 * --------------------------------------------------------------------- */

void vibe_sched_lock(void)
{
    vibe_irq_key_t key = arch_irq_lock();
    g_sched_lock_count++;
    arch_irq_unlock(key);
}

void vibe_sched_unlock(void)
{
    vibe_irq_key_t key = arch_irq_lock();
    if (g_sched_lock_count > 0) {
        g_sched_lock_count--;
    }
    arch_irq_unlock(key);

    /* If we just unlocked and a better thread is ready, reschedule. */
    if (g_sched_lock_count == 0) {
        _vibe_sched_reschedule();
    }
}

int vibe_sched_lock_count(void)
{
    return g_sched_lock_count;
}

/* -----------------------------------------------------------------------
 * Scheduler mode
 * --------------------------------------------------------------------- */

void vibe_sched_set_mode(vibe_sched_mode_t mode)
{
    g_sched_mode = mode;

    switch (mode) {
    case VIBE_SCHED_LOW_POWER:
        /* Reduce tick rate to save power.
         * USER: call arch_set_next_tick() with a longer period. */
        break;
    case VIBE_SCHED_PERFORMANCE:
        /* Run at full tick rate.
         * USER: ensure CONFIG_SYS_CLOCK_HZ tick is always on. */
        break;
    case VIBE_SCHED_MID:
    default:
        /* Balanced default — tickless idle handles low-power periods. */
        break;
    }
}

vibe_sched_mode_t vibe_sched_get_mode(void)
{
    return g_sched_mode;
}

/* -----------------------------------------------------------------------
 * Scheduler start — called once from vibe_init(), never returns
 * --------------------------------------------------------------------- */

void vibe_sched_start(void)
{
    /* NOTE: run queues and the sleep list are NOT re-initialised here.
     * A zeroed vibe_list_t (from BSS) is already a valid empty list, and
     * threads created during vibe_init() — idle thread, system work
     * queue, application threads — are enqueued BEFORE this function
     * runs. Re-initialising the queues here would silently drop them
     * (that was the "FATAL: no thread to schedule" bug). */

    /* Select the first thread to run. */
    g_current_thread = _vibe_sched_next_thread();
    if (g_current_thread == NULL) {
        g_current_thread = g_idle_thread;
    }

    if (g_current_thread == NULL) {
        vibe_printk("FATAL: no thread to schedule\n");
        for (;;) {}
    }

    g_current_thread->state = VIBE_THREAD_RUNNING;

#ifdef CONFIG_TRACE
    g_current_thread->switch_in_count++;
    g_current_thread->last_switch_in = vibe_tick_get();
#endif
    VIBE_TRACE_THREAD_SWITCH_IN(g_current_thread);

    /* Perform the very first context switch from "no thread" to the
     * selected thread. arch_switch_to(NULL, first) pends PendSV, which
     * initialises the CPU state from the thread's pre-built stack frame
     * as soon as interrupts allow it to fire. */
    arch_switch_to(NULL, g_current_thread);

    /* Never reached. */
    for (;;) {}
}

/* -----------------------------------------------------------------------
 * Accessors used by other kernel modules
 * --------------------------------------------------------------------- */

vibe_thread_t *vibe_thread_self(void)
{
    return g_current_thread;
}
