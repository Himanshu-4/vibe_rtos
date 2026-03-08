#ifndef VIBE_IRQ_H
#define VIBE_IRQ_H

/**
 * @file irq.h
 * @brief IRQ management API for VibeRTOS.
 *
 * Provides portable interrupt locking, IRQ connection, and NVIC control.
 * Arch-specific implementation lives in arch/arm/cortex_m/core/irq.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * IRQ key type
 * --------------------------------------------------------------------- */

/** Opaque key returned by vibe_irq_lock() to restore interrupt state. */
typedef uint32_t vibe_irq_key_t;

/* -----------------------------------------------------------------------
 * IRQ handler type
 * --------------------------------------------------------------------- */

/** IRQ handler function signature. */
typedef void (*vibe_irq_handler_t)(void *arg);

/* -----------------------------------------------------------------------
 * Global IRQ lock / unlock
 * --------------------------------------------------------------------- */

/**
 * @brief Disable all maskable interrupts and return the previous state.
 *
 * On Cortex-M: executes CPSID I (sets PRIMASK).
 * Calls can be nested — each call saves the previous PRIMASK value.
 *
 * @return Key encoding the previous interrupt state.
 */
vibe_irq_key_t vibe_irq_lock(void);

/**
 * @brief Restore interrupt state from a previously saved key.
 *
 * On Cortex-M: restores PRIMASK to the value captured by vibe_irq_lock().
 *
 * @param key  Value returned by a prior vibe_irq_lock() call.
 */
void vibe_irq_unlock(vibe_irq_key_t key);

/* -----------------------------------------------------------------------
 * ISR detection
 * --------------------------------------------------------------------- */

/**
 * @brief Check whether the caller is executing inside an interrupt handler.
 *
 * On Cortex-M: reads the IPSR register (non-zero means in ISR).
 *
 * @return true if in ISR context, false if in thread context.
 */
bool vibe_irq_is_in_isr(void);

/* -----------------------------------------------------------------------
 * IRQ connect / enable / disable
 * --------------------------------------------------------------------- */

/**
 * @brief Connect an ISR to a hardware IRQ line.
 *
 * Registers handler as the ISR for the given IRQ number and sets its
 * NVIC priority. The IRQ is NOT enabled automatically — call vibe_irq_enable().
 *
 * @param irq      IRQ line number (device-specific).
 * @param priority NVIC priority (0 = highest, 255 = lowest).
 * @param handler  ISR function to register.
 * @param arg      Argument passed to handler when invoked.
 * @return         VIBE_OK or VIBE_EINVAL.
 */
vibe_err_t vibe_irq_connect(uint32_t           irq,
                              uint32_t           priority,
                              vibe_irq_handler_t handler,
                              void              *arg);

/**
 * @brief Enable a previously connected IRQ in the NVIC.
 *
 * @param irq  IRQ line number.
 */
void vibe_irq_enable(uint32_t irq);

/**
 * @brief Disable (mask) an IRQ in the NVIC.
 *
 * @param irq  IRQ line number.
 */
void vibe_irq_disable(uint32_t irq);

/**
 * @brief Check whether an IRQ is currently enabled in the NVIC.
 *
 * @param irq  IRQ line number.
 * @return     true if enabled.
 */
bool vibe_irq_is_enabled(uint32_t irq);

/**
 * @brief Set the priority of an IRQ in the NVIC.
 *
 * @param irq       IRQ line number.
 * @param priority  New priority (0 = highest).
 */
void vibe_irq_set_priority(uint32_t irq, uint32_t priority);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_IRQ_H */
