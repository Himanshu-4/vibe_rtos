#ifndef VIBE_SEM_H
#define VIBE_SEM_H

/**
 * @file sem.h
 * @brief Counting semaphore API for VibeRTOS.
 *
 * A counting semaphore with a configurable maximum count.
 * Can be used as a binary semaphore by setting max_count = 1.
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Semaphore struct
 * --------------------------------------------------------------------- */

typedef struct vibe_sem {
    uint32_t     count;      /**< Current semaphore count. */
    uint32_t     max_count;  /**< Maximum count (0 = unlimited). */
    vibe_list_t  wait_list;  /**< Threads waiting to take. */
} vibe_sem_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_SEM_DEFINE — statically define and initialise a semaphore.
 *
 * @param name     C identifier.
 * @param initial  Initial count value.
 * @param max      Maximum count (0 = no upper limit).
 */
#define VIBE_SEM_DEFINE(name, initial, max) \
    vibe_sem_t name = {                      \
        .count     = (initial),              \
        .max_count = (max),                  \
    }

/* -----------------------------------------------------------------------
 * Semaphore API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a semaphore.
 *
 * @param sem      Semaphore to initialise.
 * @param initial  Initial count.
 * @param max      Maximum count (0 = unlimited).
 * @return         VIBE_OK or VIBE_EINVAL.
 */
vibe_err_t vibe_sem_init(vibe_sem_t *sem, uint32_t initial, uint32_t max);

/**
 * @brief Take (decrement) a semaphore, blocking if count is zero.
 *
 * @param sem      Semaphore to take.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or tick count.
 * @return         VIBE_OK on success, VIBE_ETIMEOUT, VIBE_EAGAIN.
 */
vibe_err_t vibe_sem_take(vibe_sem_t *sem, vibe_tick_t timeout);

/**
 * @brief Give (increment) a semaphore, waking a waiter if any.
 *
 * If a thread is waiting, it is made READY instead of incrementing the count.
 * If count is already at max_count and no waiter, returns VIBE_EOVERFLOW.
 *
 * Safe to call from ISR context.
 *
 * @param sem  Semaphore to give.
 * @return     VIBE_OK or VIBE_EOVERFLOW.
 */
vibe_err_t vibe_sem_give(vibe_sem_t *sem);

/**
 * @brief Return the current semaphore count.
 *
 * @param sem  Semaphore to query.
 * @return     Current count value.
 */
uint32_t vibe_sem_count(const vibe_sem_t *sem);

/**
 * @brief Reset the semaphore count to zero, releasing no waiters.
 *
 * @param sem  Semaphore to reset.
 */
void vibe_sem_reset(vibe_sem_t *sem);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SEM_H */
