#ifndef VIBE_TYPES_H
#define VIBE_TYPES_H

/**
 * @file types.h
 * @brief Core type definitions for VibeRTOS.
 *
 * Provides the fundamental types, error codes, and constants used
 * throughout the entire kernel and driver stack.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Basic kernel types
 * --------------------------------------------------------------------- */

/** System tick counter type. Wraps at UINT32_MAX (~49 days at 1kHz). */
typedef uint32_t vibe_tick_t;

/** Standard kernel error/return code type. */
typedef int32_t vibe_err_t;

/** CPU register-width unsigned integer (for IRQ keys, stack words, etc.). */
typedef uint32_t vibe_reg_t;

/* -----------------------------------------------------------------------
 * Error codes
 * --------------------------------------------------------------------- */

#define VIBE_OK        ((vibe_err_t)  0)   /**< Success */
#define VIBE_ETIMEOUT  ((vibe_err_t) -1)   /**< Operation timed out */
#define VIBE_ENOMEM    ((vibe_err_t) -2)   /**< Out of memory */
#define VIBE_EINVAL    ((vibe_err_t) -3)   /**< Invalid argument */
#define VIBE_EBUSY     ((vibe_err_t) -4)   /**< Resource busy */
#define VIBE_EAGAIN    ((vibe_err_t) -5)   /**< Try again / would block */
#define VIBE_ENOSYS    ((vibe_err_t) -6)   /**< Not implemented */
#define VIBE_ENODEV    ((vibe_err_t) -7)   /**< No such device */
#define VIBE_EPERM     ((vibe_err_t) -8)   /**< Operation not permitted */
#define VIBE_EDEADLK   ((vibe_err_t) -9)   /**< Deadlock would occur */
#define VIBE_EOVERFLOW ((vibe_err_t) -10)  /**< Value overflows the type */

/* -----------------------------------------------------------------------
 * Wait / timeout constants
 * --------------------------------------------------------------------- */

/** Block forever — pass as timeout to any blocking API. */
#define VIBE_WAIT_FOREVER  ((vibe_tick_t)UINT32_MAX)

/** Do not block — return immediately if the resource is unavailable. */
#define VIBE_NO_WAIT       ((vibe_tick_t)0)

/* -----------------------------------------------------------------------
 * Convenience helpers
 * --------------------------------------------------------------------- */

/** Convert milliseconds to ticks (requires CONFIG_SYS_CLOCK_HZ). */
#ifndef CONFIG_SYS_CLOCK_HZ
#define CONFIG_SYS_CLOCK_HZ 1000
#endif

#define VIBE_MS_TO_TICKS(ms)   ((vibe_tick_t)(((uint64_t)(ms) * CONFIG_SYS_CLOCK_HZ) / 1000U))
#define VIBE_TICKS_TO_MS(t)    ((uint32_t)(((uint64_t)(t) * 1000U) / CONFIG_SYS_CLOCK_HZ))

/** Alignment attribute shorthand. */
#define VIBE_ALIGNED(x)    __attribute__((aligned(x)))

/** Mark a function/variable as used (prevent linker garbage collection). */
#define VIBE_USED          __attribute__((used))

/** Weak symbol — can be overridden by application. */
#define VIBE_WEAK          __attribute__((weak))

/** Mark a function as not returning. */
#define VIBE_NORETURN      __attribute__((noreturn))

/** Place symbol in a named linker section. */
#define VIBE_SECTION(s)    __attribute__((section(s)))

/** Pack a struct with no padding. */
#define VIBE_PACKED        __attribute__((packed))

#ifdef __cplusplus
}
#endif

#endif /* VIBE_TYPES_H */
