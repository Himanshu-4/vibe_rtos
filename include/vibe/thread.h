#ifndef VIBE_THREAD_H
#define VIBE_THREAD_H

/**
 * @file thread.h
 * @brief Thread management API for VibeRTOS.
 *
 * Provides thread creation, deletion, state control, priority management,
 * sleep/yield, and the thread restart mechanism.
 */

#include <stdint.h>
#include <stdbool.h>
#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Thread priority constants
 * --------------------------------------------------------------------- */

/** Lowest possible thread priority (idle thread runs at this level). */
#define VIBE_PRIO_MIN    0

/** Highest possible thread priority. */
#define VIBE_PRIO_MAX    31

/** Default priority for application threads. */
#define VIBE_PRIO_DEFAULT  16

/** Priority of the system work queue thread. */
#define VIBE_PRIO_SYS_WORKQ  (VIBE_PRIO_MAX - 1)

/* -----------------------------------------------------------------------
 * Thread states
 * --------------------------------------------------------------------- */

typedef enum {
    VIBE_THREAD_READY     = 0,  /**< Ready to run, in scheduler run queue. */
    VIBE_THREAD_RUNNING   = 1,  /**< Currently executing on a CPU. */
    VIBE_THREAD_SUSPENDED = 2,  /**< Explicitly suspended, not schedulable. */
    VIBE_THREAD_SLEEPING  = 3,  /**< Waiting for a time delay to expire. */
    VIBE_THREAD_WAITING   = 4,  /**< Blocked on a sync primitive (mutex/sem/etc.). */
    VIBE_THREAD_DEAD      = 5,  /**< Entry function returned; thread is done. */
} vibe_thread_state_t;

/* -----------------------------------------------------------------------
 * Scheduler / power modes
 * --------------------------------------------------------------------- */

typedef enum {
    VIBE_SCHED_LOW_POWER   = 0, /**< Minimize CPU activity, reduce tick rate. */
    VIBE_SCHED_MID         = 1, /**< Balanced mode (default). */
    VIBE_SCHED_PERFORMANCE = 2, /**< Maximize throughput, full tick rate. */
} vibe_sched_mode_t;

/* -----------------------------------------------------------------------
 * Thread entry function type
 * --------------------------------------------------------------------- */

/** Thread entry function signature. */
typedef void (*vibe_thread_entry_t)(void *arg);

/* -----------------------------------------------------------------------
 * Thread control block (TCB)
 * --------------------------------------------------------------------- */

/**
 * @brief Thread Control Block.
 *
 * Contains all state needed to save and restore a thread's execution context.
 * Allocated either statically (via VIBE_THREAD_DEFINE) or dynamically.
 */
typedef struct vibe_thread {
    /* Identity */
    uint32_t              id;              /**< Unique thread ID. */
    char                  name[32];        /**< Human-readable name. */

    /* Scheduling state */
    vibe_thread_state_t   state;           /**< Current execution state. */
    uint8_t               priority;        /**< Current effective priority. */
    uint8_t               base_priority;   /**< Original priority (for PI restore). */

    /* Entry point */
    vibe_thread_entry_t   entry;           /**< Thread entry function. */
    void                 *arg;             /**< Argument passed to entry. */

    /* Stack */
    void                 *stack_ptr;       /**< Current stack pointer (saved on switch). */
    void                 *stack_base;      /**< Lowest byte of the stack region. */
    size_t                stack_size;      /**< Total stack size in bytes. */

    /* Timing */
    vibe_tick_t           timeout;         /**< Tick count when this thread wakes up. */

    /* List linkage — embedded in scheduler and wait-list queues. */
    vibe_list_node_t      sched_node;      /**< Node in the scheduler run queue. */
    vibe_list_node_t      wait_node;       /**< Node in a sync-primitive wait list. */
    vibe_list_node_t      sleep_node;      /**< Node in the sleep list. */

    /* Thread-local storage */
    void                 *tls;             /**< Pointer to per-thread TLS block. */

    /* SMP */
    uint8_t               cpu_id;          /**< CPU this thread is pinned/running on. */

    /* Flags */
    uint8_t               is_static : 1;   /**< Stack/TCB allocated statically. */
    uint8_t               detached  : 1;   /**< Auto-delete on exit. */
    uint8_t               reserved  : 6;
} vibe_thread_t;

/* -----------------------------------------------------------------------
 * Thread attribute struct (used at creation time)
 * --------------------------------------------------------------------- */

typedef struct {
    size_t      stack_size;   /**< Stack size in bytes (0 = use default). */
    uint8_t     priority;     /**< Initial priority. */
    const char *name;         /**< Thread name (may be NULL). */
    bool        is_static;    /**< True if stack/TCB are statically allocated. */
    int         cpu_id;       /**< CPU affinity (-1 = any). */
} vibe_thread_attr_t;

/* -----------------------------------------------------------------------
 * Static thread definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_THREAD_DEFINE — statically define a thread with its own stack.
 *
 * Declares and initialises the TCB and stack buffer at compile time.
 * The thread must still be started with vibe_thread_create().
 *
 * @param tname       C identifier for the thread variable.
 * @param stack_sz    Stack size in bytes.
 * @param entry_fn    Thread entry function (void fn(void *arg)).
 * @param user_arg    Argument passed to entry_fn.
 * @param prio        Initial priority (VIBE_PRIO_MIN .. VIBE_PRIO_MAX).
 */
#define VIBE_THREAD_DEFINE(tname, stack_sz, entry_fn, user_arg, prio)   \
    static uint8_t _##tname##_stack[stack_sz]                           \
        __attribute__((aligned(8), used));                               \
    static vibe_thread_t tname                                           \
        __attribute__((used)) = {                                        \
        .name          = #tname,                                         \
        .entry         = (entry_fn),                                     \
        .arg           = (user_arg),                                     \
        .stack_base    = _##tname##_stack,                               \
        .stack_size    = (stack_sz),                                     \
        .priority      = (prio),                                         \
        .base_priority = (prio),                                         \
        .state         = VIBE_THREAD_SUSPENDED,                          \
        .is_static     = 1,                                              \
    }

/* -----------------------------------------------------------------------
 * Thread API
 * --------------------------------------------------------------------- */

/**
 * @brief Create and start a new thread.
 *
 * @param[out] thread   Pointer to a vibe_thread_t to fill (or NULL to alloc).
 * @param      entry    Thread entry function.
 * @param      arg      Argument passed to entry.
 * @param      attr     Thread attributes (stack size, priority, name).
 * @param      stack    Pre-allocated stack buffer (NULL = allocate from heap).
 * @return     VIBE_OK on success, negative error code on failure.
 */
vibe_err_t vibe_thread_create(vibe_thread_t          *thread,
                               vibe_thread_entry_t     entry,
                               void                   *arg,
                               const vibe_thread_attr_t *attr,
                               void                   *stack);

/**
 * @brief Delete a thread and release its resources (if dynamically allocated).
 *
 * @param thread  Thread to delete. If NULL, deletes the calling thread.
 * @return        VIBE_OK on success.
 */
vibe_err_t vibe_thread_delete(vibe_thread_t *thread);

/**
 * @brief Suspend a thread — removes it from the run queue.
 *
 * A suspended thread is not schedulable until vibe_thread_resume() is called.
 * A thread may suspend itself (thread == NULL or thread == current).
 *
 * @param thread  Thread to suspend.
 * @return        VIBE_OK, or VIBE_EINVAL if thread is already dead.
 */
vibe_err_t vibe_thread_suspend(vibe_thread_t *thread);

/**
 * @brief Resume a suspended thread — places it back in the run queue.
 *
 * @param thread  Thread to resume.
 * @return        VIBE_OK, or VIBE_EINVAL if not in SUSPENDED state.
 */
vibe_err_t vibe_thread_resume(vibe_thread_t *thread);

/**
 * @brief Put the calling thread to sleep for the given number of milliseconds.
 *
 * @param ms  Sleep duration in milliseconds.
 */
void vibe_thread_sleep(uint32_t ms);

/**
 * @brief Yield the CPU to another ready thread at the same or higher priority.
 *
 * If no other thread is ready, returns immediately.
 */
void vibe_thread_yield(void);

/**
 * @brief Restart a thread from its entry point.
 *
 * Resets the thread's stack and re-executes its entry function.
 * Can be called on self or another thread.
 * The thread is re-enqueued after restart.
 *
 * @param thread  Thread to restart (NULL = self).
 * @return        VIBE_OK on success.
 */
vibe_err_t vibe_thread_restart(vibe_thread_t *thread);

/**
 * @brief Change a thread's priority.
 *
 * Supports dynamic priority switching. If the thread is blocked in a
 * mutex wait list and the new priority is higher, priority inheritance
 * is re-evaluated.
 *
 * @param thread    Thread to reprioritise.
 * @param priority  New priority value (VIBE_PRIO_MIN .. VIBE_PRIO_MAX).
 * @return          VIBE_OK, or VIBE_EINVAL for out-of-range priority.
 */
vibe_err_t vibe_thread_set_priority(vibe_thread_t *thread, uint8_t priority);

/**
 * @brief Get the currently running thread on the calling CPU.
 *
 * @return Pointer to the current vibe_thread_t.
 */
vibe_thread_t *vibe_thread_self(void);

/**
 * @brief Check whether the caller is executing in ISR context.
 *
 * @return true if in ISR, false if in thread context.
 */
bool vibe_thread_is_in_isr(void);

/**
 * @brief Get a thread's current state.
 */
vibe_thread_state_t vibe_thread_get_state(const vibe_thread_t *thread);

/**
 * @brief Get a thread's name string.
 */
const char *vibe_thread_get_name(const vibe_thread_t *thread);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_THREAD_H */
