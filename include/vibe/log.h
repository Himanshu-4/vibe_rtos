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
 * Module registration macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_LOG_MODULE_REGISTER — register a log module for this translation unit.
 *
 * Must be placed at file scope, outside any function.
 *
 * @param mod_name   Identifier for the module (also used as string).
 * @param init_level Initial log level (vibe_log_level_t).
 */
#define VIBE_LOG_MODULE_REGISTER(mod_name, init_level)                    \
    static vibe_log_module_t _log_module_##mod_name                        \
        __attribute__((used, section("._vibe_log_modules"))) = {           \
        .name  = #mod_name,                                                \
        .level = (init_level),                                             \
    };                                                                     \
    static vibe_log_module_t *const _log_mod = &_log_module_##mod_name

/* -----------------------------------------------------------------------
 * Internal log emit function (do not call directly)
 * --------------------------------------------------------------------- */

void _vibe_log_emit(vibe_log_module_t *mod,
                    vibe_log_level_t   level,
                    const char        *file,
                    int                line,
                    const char        *fmt,
                    ...) __attribute__((format(printf, 5, 6)));

/* -----------------------------------------------------------------------
 * Logging macros
 * --------------------------------------------------------------------- */

#ifndef CONFIG_LOG_DEFAULT_LEVEL
#define CONFIG_LOG_DEFAULT_LEVEL VIBE_LOG_LEVEL_INF
#endif

/**
 * Log an error message (always compiled in unless CONFIG_LOG is off).
 */
#define VIBE_LOG_ERR(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_ERR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * Log a warning message.
 */
#define VIBE_LOG_WRN(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_WRN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * Log an informational message.
 */
#define VIBE_LOG_INF(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_INF, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * Log a debug message.
 */
#define VIBE_LOG_DBG(fmt, ...) \
    _vibe_log_emit(_log_mod, VIBE_LOG_LEVEL_DBG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* -----------------------------------------------------------------------
 * Runtime level control
 * --------------------------------------------------------------------- */

/**
 * @brief Change the log level for a named module at runtime.
 *
 * @param module_name  Module name string (as given to VIBE_LOG_MODULE_REGISTER).
 * @param level        New log level.
 * @return             VIBE_OK, or VIBE_ENODEV if module not found.
 */
vibe_err_t vibe_log_set_level(const char *module_name, vibe_log_level_t level);

/**
 * @brief Get the current log level for a named module.
 *
 * @param module_name  Module name.
 * @return             Current level, or VIBE_LOG_LEVEL_NONE if not found.
 */
vibe_log_level_t vibe_log_get_level(const char *module_name);

/**
 * @brief Flush any buffered/deferred log messages to the backend.
 *
 * A no-op when CONFIG_LOG_DEFERRED is not enabled.
 */
void vibe_log_flush(void);

/* -----------------------------------------------------------------------
 * Backend initialisation (called by kernel init)
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise the logging subsystem and registered backends.
 *
 * Called from vibe_init() after the UART/RTT drivers are ready.
 */
void _vibe_log_init(void);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_LOG_H */
