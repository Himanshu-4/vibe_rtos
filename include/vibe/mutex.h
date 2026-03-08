#ifndef VIBE_MUTEX_H
#define VIBE_MUTEX_H

/**
 * @file mutex.h
 * @brief Mutex API with priority inheritance for VibeRTOS.
 *
 * Supports recursive locking and priority inheritance to prevent
 * priority inversion when a high-priority thread waits for a mutex
 * held by a lower-priority thread.
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration to avoid circular includes. */
struct vibe_thread;

/* -----------------------------------------------------------------------
 * Mutex struct
 * --------------------------------------------------------------------- */

typedef struct vibe_mutex {
    struct vibe_thread *owner;          /**< Thread currently holding the mutex. */
    uint32_t            lock_count;     /**< Recursive lock depth (0 = unlocked). */
    vibe_list_t         wait_list;      /**< Threads blocked waiting for this mutex. */
    uint8_t             owner_orig_prio;/**< Owner's original priority (for PI restore). */
} vibe_mutex_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_MUTEX_DEFINE — statically define and initialise a mutex.
 *
 * @param name  C identifier for the mutex variable.
 */
#define VIBE_MUTEX_DEFINE(name)  \
    vibe_mutex_t name = {        \
        .owner           = NULL, \
        .lock_count      = 0U,   \
        .owner_orig_prio = 0U,   \
    }

/* -----------------------------------------------------------------------
 * Mutex API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a mutex at runtime.
 *
 * Use VIBE_MUTEX_DEFINE for static allocation or call this for
 * dynamically allocated mutexes.
 *
 * @param mutex  Pointer to mutex to initialise.
 * @return       VIBE_OK.
 */
vibe_err_t vibe_mutex_init(vibe_mutex_t *mutex);

/**
 * @brief Lock a mutex, blocking for up to timeout ticks.
 *
 * - If unlocked: take immediately.
 * - If locked by the calling thread: increment recursive count.
 * - If locked by another thread: apply priority inheritance (boost owner
 *   priority if needed), then block for up to timeout ticks.
 *
 * @param mutex    Mutex to lock.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or ticks.
 * @return         VIBE_OK on success, VIBE_ETIMEOUT, VIBE_EDEADLK, etc.
 */
vibe_err_t vibe_mutex_lock(vibe_mutex_t *mutex, vibe_tick_t timeout);

/**
 * @brief Unlock a mutex.
 *
 * Decrements the recursive count. When it reaches zero:
 * - Restores the original owner priority (undo PI boost).
 * - Wakes the highest-priority waiter, transferring ownership.
 *
 * @param mutex  Mutex to unlock.
 * @return       VIBE_OK, or VIBE_EPERM if the caller does not own the mutex.
 */
vibe_err_t vibe_mutex_unlock(vibe_mutex_t *mutex);

/**
 * @brief Try to lock a mutex without blocking.
 *
 * Equivalent to vibe_mutex_lock(mutex, VIBE_NO_WAIT).
 *
 * @param mutex  Mutex to try-lock.
 * @return       VIBE_OK if acquired, VIBE_EBUSY if already held.
 */
vibe_err_t vibe_mutex_trylock(vibe_mutex_t *mutex);

/**
 * @brief Check whether the calling thread owns the mutex.
 *
 * @param mutex  Mutex to query.
 * @return       true if the caller is the owner.
 */
bool vibe_mutex_is_owner(const vibe_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_MUTEX_H */
