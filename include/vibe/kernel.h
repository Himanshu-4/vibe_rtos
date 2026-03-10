#ifndef VIBE_KERNEL_H
#define VIBE_KERNEL_H

/**
 * @file kernel.h
 * @brief VibeRTOS umbrella header.
 *
 * Including this single header pulls in the entire public kernel API.
 * Application code typically includes only this file.
 *
 * Example:
 *   #include <vibe/kernel.h>
 *
 *   VIBE_THREAD_DEFINE(my_thread, 1024, my_entry, NULL, 16);
 *
 *   int main(void) {
 *       vibe_init();
 *       // never returns
 *   }
 */

/* Version information */
#define VIBE_RTOS_VERSION_MAJOR  0
#define VIBE_RTOS_VERSION_MINOR  1
#define VIBE_RTOS_VERSION_PATCH  0

#define VIBE_RTOS_VERSION_STRING "0.1.0"

/* Core types and utilities */
#include "vibe/types.h"
#include "vibe/sys/util.h"
#include "vibe/sys/list.h"
#include "vibe/sys/printk.h"

/* Kernel subsystems */
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/timer.h"
#include "vibe/workq.h"

/* Synchronisation primitives */
#include "vibe/mutex.h"
#include "vibe/sem.h"
#include "vibe/event.h"
#include "vibe/msgq.h"
#include "vibe/pipe.h"

/* Memory management */
#include "vibe/mem.h"
#include "vibe/spinlock.h"

/* Hardware abstraction */
#include "vibe/irq.h"
#include "vibe/device.h"

/* Logging */
#include "vibe/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Kernel lifecycle
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise and start VibeRTOS.
 *
 * Must be called from main() (or the startup code) after hardware
 * initialisation is complete. Performs the following in order:
 *
 *  1. arch_init() — architecture-specific setup.
 *  2. Device init for PRE_KERNEL_1 and PRE_KERNEL_2 levels.
 *  3. Kernel object initialisation (scheduler, timer list, etc.).
 *  4. Idle thread and system work queue creation.
 *  5. Device init for POST_KERNEL level.
 *  6. Logging subsystem init.
 *  7. vibe_sched_start() — enters the scheduler, never returns.
 *
 * @note This function never returns.
 */
void vibe_init(void) __attribute__((noreturn));

/* -----------------------------------------------------------------------
 * System tick and time
 * --------------------------------------------------------------------- */

/**
 * @brief Return the raw system tick counter.
 *
 * Monotonically increasing. Wraps at UINT32_MAX.
 *
 * @return Current tick value.
 */
#ifndef VIBE_TICK_GET_DECLARED
#define VIBE_TICK_GET_DECLARED
vibe_tick_t vibe_tick_get(void);
#endif

/**
 * @brief Return system uptime in milliseconds.
 *
 * @return Milliseconds since vibe_init() was called.
 */
uint32_t vibe_uptime_ms(void);

/* -----------------------------------------------------------------------
 * System control
 * --------------------------------------------------------------------- */

/**
 * @brief Reboot the system.
 *
 * Calls the arch-specific reboot implementation.
 * The default implementation (weak) triggers a NVIC system reset on ARM.
 */
void vibe_reboot(void) __attribute__((weak, noreturn));

#ifdef __cplusplus
}
#endif

#endif /* VIBE_KERNEL_H */
