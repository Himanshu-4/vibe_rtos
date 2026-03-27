#ifndef VIBE_LOG_H
#define VIBE_LOG_H

/**
 * @file log.h
 * @brief Logging subsystem API for VibeRTOS.
 *
 * Provides levelled, module-scoped logging with multiple backends
 * (UART, RTT, memory ring buffer) and optional deferred (async) output.
 *
 * Usage:
 *   // At the top of your .c file:
 *   VIBE_LOG_MODULE_REGISTER(my_module, VIBE_LOG_LEVEL_INF);
 *
 *   // Then anywhere in that file:
 *   VIBE_LOG_INF("temperature = %d C", temp);
 *   VIBE_LOG_ERR("init failed: %d", err);
 */

#include <stdint.h>
#include <stdarg.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Log levels
 * --------------------------------------------------------------------- */

typedef enum {
    VIBE_LOG_LEVEL_ERR = 0,  /**< Errors only. */
    VIBE_LOG_LEVEL_WRN = 1,  /**< Warnings and errors. */
    VIBE_LOG_LEVEL_INF = 2,  /**< Info, warnings, errors. */
    VIBE_LOG_LEVEL_DBG = 3,  /**< All messages including debug. */
    VIBE_LOG_LEVEL_NONE = 4, /**< Disable all logging for this module. */
} vibe_log_level_t;

/* -----------------------------------------------------------------------
 * Module descriptor
 * --------------------------------------------------------------------- */

typedef struct vibe_log_module {
    const char       *name;          /**< Module name string. */
    vibe_log_level_t  level;         /**< Current filter level. */
} vibe_log_module_t;

/* -----------------------------------------------------------------------
 * When CONFIG_LOG is enabled: full API.
 * When CONFIG_LOG is disabled: all macros and init functions compile away
 * to nothing so callers (kernel, drivers) need no #ifdef guards.
 * --------------------------------------------------------------------- */

#ifdef CONFIG_LOG

#define VIBE_LOG_MODULE_REGISTER(mod_name, init_level)                    \
    static vibe_log_module_t _log_module_##mod_name                        \
        __attribute__((used, section("._vibe_log_modules"))) = {           \
        .name  = #mod_name,                                                \
        .level = (init_level),                                             \
    };                                                                     \
    static vibe_log_module_t *const _log_mod = &_log_module_##mod_name

void _vibe_log_emit(vibe_log_module_t *mod,
                    vibe_log_level_t   level,
                    const char        *file,
                    int                line,
                    const char        *fmt,
                    ...) __attribute__((format(printf, 5, 6)));

#ifndef CONFIG_LOG_DEFAULT_LEVEL
#define CONFIG_LOG_DEFAULT_LEVEL VIBE_LOG_LEVEL_INF
#endif

#define VIBE_LOG_ERR(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_ERR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VIBE_LOG_WRN(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_WRN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VIBE_LOG_INF(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_INF, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VIBE_LOG_DBG(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_DBG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

vibe_err_t       vibe_log_set_level(const char *module_name, vibe_log_level_t level);
vibe_log_level_t vibe_log_get_level(const char *module_name);
void             vibe_log_flush(void);
void             _vibe_log_init(void);

#else /* CONFIG_LOG disabled — compile everything away */

#define VIBE_LOG_MODULE_REGISTER(mod_name, init_level)  /* disabled */
#define VIBE_LOG_ERR(fmt, ...)   do {} while (0)
#define VIBE_LOG_WRN(fmt, ...)   do {} while (0)
#define VIBE_LOG_INF(fmt, ...)   do {} while (0)
#define VIBE_LOG_DBG(fmt, ...)   do {} while (0)

static inline void             _vibe_log_init(void) {}
static inline void             vibe_log_flush(void) {}
static inline vibe_log_level_t vibe_log_get_level(const char *n)
    { (void)n; return VIBE_LOG_LEVEL_NONE; }
static inline vibe_err_t       vibe_log_set_level(const char *n, vibe_log_level_t l)
    { (void)n; (void)l; return 0; }

#endif /* CONFIG_LOG */

#ifdef __cplusplus
}
#endif

#endif /* VIBE_LOG_H */
