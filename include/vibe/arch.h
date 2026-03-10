#ifndef VIBE_ARCH_H
#define VIBE_ARCH_H

/**
 * @file vibe/arch.h
 * @brief Architecture abstraction selector.
 *
 * Kernel code includes this header instead of any arch-specific path.
 * The correct arch header is selected based on the build configuration.
 * Also pulls in irq.h so vibe_irq_key_t is always available alongside
 * the arch_irq_lock/unlock declarations.
 */

#include "vibe/types.h"
#include "vibe/irq.h"

#if defined(CONFIG_ARCH_ARM) || defined(__ARM_ARCH)
    #include "arch/arm/cortex_m/arch.h"
#else
    #error "No supported architecture detected. Define CONFIG_ARCH_ARM."
#endif

/* Declare the fundamental tick accessor here so kernel source files
 * can call vibe_tick_get() without pulling in the full kernel.h umbrella
 * (which would create circular include chains). */
#ifndef VIBE_TICK_GET_DECLARED
#define VIBE_TICK_GET_DECLARED
#include "vibe/types.h"
vibe_tick_t vibe_tick_get(void);
#endif

#endif /* VIBE_ARCH_H */
