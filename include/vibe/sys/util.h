#ifndef VIBE_SYS_UTIL_H
#define VIBE_SYS_UTIL_H

/**
 * @file sys/util.h
 * @brief General-purpose utility macros for VibeRTOS.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Array and struct helpers
 * --------------------------------------------------------------------- */

/** Number of elements in a statically-sized array. */
#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

/**
 * CONTAINER_OF — get pointer to the containing struct from a member pointer.
 *
 * @param ptr    Pointer to the member field.
 * @param type   Type of the containing struct.
 * @param member Name of the member field within the struct.
 */
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* -----------------------------------------------------------------------
 * Alignment
 * --------------------------------------------------------------------- */

/** Round x up to the next multiple of alignment a (a must be power of 2). */
#define ALIGN_UP(x, a)    (((x) + ((a) - 1U)) & ~((a) - 1U))

/** Round x down to the previous multiple of alignment a (a must be power of 2). */
#define ALIGN_DOWN(x, a)  ((x) & ~((a) - 1U))

/** Check whether x is aligned to a (a must be power of 2). */
#define IS_ALIGNED(x, a)  (((x) & ((a) - 1U)) == 0U)

/* -----------------------------------------------------------------------
 * Bit manipulation
 * --------------------------------------------------------------------- */

/** Bit mask with bit n set. */
#define BIT(n)            (1UL << (n))

/** Mask of the lowest n bits. */
#define BITS(n)           (BIT(n) - 1UL)

/** Set bits in mask in register reg. */
#define BIT_SET(reg, mask)    ((reg) |= (mask))

/** Clear bits in mask in register reg. */
#define BIT_CLR(reg, mask)    ((reg) &= ~(mask))

/** Toggle bits in mask in register reg. */
#define BIT_TOGGLE(reg, mask) ((reg) ^= (mask))

/** Test whether any bit in mask is set in reg. */
#define BIT_IS_SET(reg, mask) (((reg) & (mask)) != 0U)

/* -----------------------------------------------------------------------
 * Math helpers
 * --------------------------------------------------------------------- */

/** Minimum of two values. */
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

/** Maximum of two values. */
#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

/** Clamp val to the range [lo, hi]. */
#define CLAMP(val, lo, hi)  (MAX((lo), MIN((val), (hi))))

/** Absolute value. */
#define ABS(x)  (((x) < 0) ? -(x) : (x))

/** Check whether x is a power of two (x > 0). */
#define IS_POWER_OF_TWO(x)  (((x) != 0U) && (((x) & ((x) - 1U)) == 0U))

/** Integer division rounding up. */
#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1U) / (d))

/* -----------------------------------------------------------------------
 * Compiler hints
 * --------------------------------------------------------------------- */

/** Hint to the compiler that cond is expected to be true. */
#define LIKELY(cond)    __builtin_expect(!!(cond), 1)

/** Hint to the compiler that cond is expected to be false. */
#define UNLIKELY(cond)  __builtin_expect(!!(cond), 0)

/** Suppress unused-variable warning. */
#define UNUSED(x)       ((void)(x))

/** Static (compile-time) assertion. */
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* -----------------------------------------------------------------------
 * String / token helpers
 * --------------------------------------------------------------------- */

#define STRINGIFY(x)    #x
#define TOSTRING(x)     STRINGIFY(x)
#define CONCAT(a, b)    a##b
#define CONCAT3(a,b,c)  a##b##c

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SYS_UTIL_H */
