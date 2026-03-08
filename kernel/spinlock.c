/**
 * @file kernel/spinlock.c
 * @brief IRQ-save spinlock implementation for VibeRTOS.
 *
 * Provides the non-inline variants of the spinlock API that disable
 * interrupts atomically alongside the spin acquire/release.  The inline
 * vibe_spinlock_lock / vibe_spinlock_unlock (defined in spinlock.h) are
 * used internally; the functions here layer IRQ masking on top via the
 * arch_irq_lock / arch_irq_unlock interface.
 *
 * On a single-core system with interrupt masking the spin loop is
 * effectively a no-op because no other code can run while IRQs are off,
 * but the API is kept symmetric with the SMP case.
 */

#include "vibe/spinlock.h"
#include "vibe/irq.h"

/* Pull in the arch IRQ lock/unlock primitives. */
extern uint32_t arch_irq_lock(void);
extern void     arch_irq_unlock(uint32_t key);

/* -----------------------------------------------------------------------
 * vibe_spinlock_lock_irqsave()
 *
 * Disable interrupts first so that the spin loop cannot be pre-empted by
 * an ISR that also tries to take the same lock (which would deadlock on a
 * single-core system).
 * --------------------------------------------------------------------- */

vibe_irq_key_t vibe_spinlock_lock_irqsave(vibe_spinlock_t *lock)
{
    vibe_irq_key_t key = arch_irq_lock();
    vibe_spinlock_lock(lock);
    return key;
}

/* -----------------------------------------------------------------------
 * vibe_spinlock_unlock_irqrestore()
 *
 * Release the spin flag before re-enabling interrupts so that another
 * CPU core (SMP) can acquire the lock immediately without waiting for
 * the IRQ window to open on this core.
 * --------------------------------------------------------------------- */

void vibe_spinlock_unlock_irqrestore(vibe_spinlock_t *lock, vibe_irq_key_t key)
{
    vibe_spinlock_unlock(lock);
    arch_irq_unlock(key);
}
