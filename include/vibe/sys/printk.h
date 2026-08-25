#ifndef VIBE_SYS_PRINTK_H
#define VIBE_SYS_PRINTK_H

/**
 * @file sys/printk.h
 * @brief Minimal kernel print and assertion facilities.
 */

#include <stdarg.h>
#include <stddef.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kernel printf — outputs formatted text over the debug UART or RTT.
 *
 * Supports a subset of printf format specifiers:
 *   %d, %i, %u, %x, %X, %o, %s, %c, %p, %%
 *
 * This function is safe to call from both thread and ISR context,
 * but may block briefly to acquire the UART TX lock.
 *
 * @param fmt  Format string (printf-style).
 * @param ...  Variable arguments.
 */
void vibe_printk(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * @brief va_list variant of vibe_printk.
 */
void vibe_vprintk(const char *fmt, va_list args);

/**
 * @brief Format into a buffer (vsnprintf-style, kernel formatter).
 *
 * Supports the same conversions as vibe_printk (%d %i %u %x %X %s %c %p %%
 * and the 'l' length modifier). The output is always NUL-terminated when
 * size > 0.
 *
 * @param buf   Destination buffer.
 * @param size  Buffer size in bytes (including space for the NUL).
 * @param fmt   Format string.
 * @param args  Argument list.
 * @return      Number of characters that would have been written
 *              (excluding the NUL), like vsnprintf.
 */
size_t vibe_vsnprintk(char *buf, size_t size, const char *fmt, va_list args);

/**
 * @brief Variadic convenience wrapper around vibe_vsnprintk().
 */
size_t vibe_snprintk(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * @brief Low-level console output hook used by vibe_printk.
 *
 * Weakly defined as a no-op in kernel/init.c. Board or SoC support code
 * provides a strong override that emits one character to the console
 * device (UART, RTT, semihosting, ...).
 */
void vibe_console_putc(char c);

/* -----------------------------------------------------------------------
 * Assertion macros
 * --------------------------------------------------------------------- */

/**
 * VIBE_ASSERT — runtime assertion.
 *
 * If cond is false, prints the file/line/message and halts.
 * In production builds (NDEBUG defined) this expands to nothing.
 */
#ifndef NDEBUG
#define VIBE_ASSERT(cond, msg)                                          \
    do {                                                                 \
        if (!(cond)) {                                                   \
            vibe_printk("ASSERT FAIL: %s\n"                             \
                        "  expr: " #cond "\n"                           \
                        "  file: " __FILE__ ":%d\n",                    \
                        (msg), __LINE__);                                \
            vibe_assert_halt();                                          \
        }                                                                \
    } while (0)
#else
#define VIBE_ASSERT(cond, msg)  ((void)(cond))
#endif

/**
 * VIBE_ASSERT_NOT_NULL — assert pointer is non-NULL.
 */
#define VIBE_ASSERT_NOT_NULL(ptr, msg) VIBE_ASSERT((ptr) != NULL, msg)

/**
 * @brief Called by VIBE_ASSERT on failure — halts the system.
 * Can be overridden by the application via weak linkage.
 */
void vibe_assert_halt(void) __attribute__((weak, noreturn));

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SYS_PRINTK_H */
