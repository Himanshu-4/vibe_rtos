#ifndef VIBE_SPINLOCK_H
#define VIBE_SPINLOCK_H

/**
 * @file spinlock.h
 * @brief Spinlock API for SMP and ISR-safe critical sections.
 *
 * Spinlocks busy-wait until the lock is available. They are suitable
 * for very short critical sections (a few instructions) that must work
 * in ISR context or across CPU cores. Never sleep inside a spinlock.
 *
 * On single-core systems the lock degenerates to an IRQ disable/enable.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Spinlock type
 * --------------------------------------------------------------------- */

typedef struct {
    atomic_flag flag;   /**< Atomic lock bit (ATOMIC_FLAG_INIT = unlocked). */
} vibe_spinlock_t;

/** Initialiser for static spinlock declaration. */
#define VIBE_SPINLOCK_INIT  { .flag = ATOMIC_FLAG_INIT }

/** Statically define an unlocked spinlock. */
#define VIBE_SPINLOCK_DEFINE(name)  vibe_spinlock_t name = VIBE_SPINLOCK_INIT

/* -----------------------------------------------------------------------
 * IRQ key type (returned by lock_irqsave)
 * --------------------------------------------------------------------- */

typedef uint32_t vibe_irq_key_t;

/* -----------------------------------------------------------------------
 * Spinlock API
 * --------------------------------------------------------------------- */

/**
 * @brief Acquire a spinlock (busy-wait).
 *
 * Does NOT disable interrupts. Use vibe_spinlock_lock_irqsave() if the
 * spinlock must also protect against ISR access.
 *
 * @param lock  Spinlock to acquire.
 */
static inline void vibe_spinlock_lock(vibe_spinlock_t *lock)
{
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire)) {
        /* Spin — on ARM insert a WFE hint to reduce bus contention on SMP. */
#if defined(__ARM_ARCH)
        __asm__ volatile("yield" ::: "memory");
#endif
    }
}

/**
 * @brief Release a spinlock.
 *
 * @param lock  Spinlock to release.
 */
static inline void vibe_spinlock_unlock(vibe_spinlock_t *lock)
{
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

/**
 * @brief Acquire a spinlock and disable interrupts atomically.
 *
 * @param lock  Spinlock to acquire.
 * @return      IRQ key — pass to vibe_spinlock_unlock_irqrestore().
 */
vibe_irq_key_t vibe_spinlock_lock_irqsave(vibe_spinlock_t *lock);

/**
 * @brief Release a spinlock and restore interrupt state.
 *
 * @param lock  Spinlock to release.
 * @param key   Key returned by vibe_spinlock_lock_irqsave().
 */
void vibe_spinlock_unlock_irqrestore(vibe_spinlock_t *lock, vibe_irq_key_t key);

/**
 * @brief Try to acquire a spinlock without spinning.
 *
 * @param lock  Spinlock to try.
 * @return      true if acquired, false if already held.
 */
static inline bool vibe_spinlock_trylock(vibe_spinlock_t *lock)
{
    return !atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire);
}

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SPINLOCK_H */
