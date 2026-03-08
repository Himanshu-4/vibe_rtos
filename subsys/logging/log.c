/**
 * @file subsys/logging/log.c
 * @brief VibeRTOS logging subsystem implementation.
 *
 * Dispatches log messages to configured backends (UART, RTT, memory).
 * Supports optional deferred (async) logging via a ring buffer flushed
 * by a low-priority log thread.
 */

#include "vibe/log.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include "vibe/spinlock.h"
#include <string.h>
#include <stdarg.h>

#ifdef CONFIG_LOG

/* -----------------------------------------------------------------------
 * Log module registry (linker section)
 * --------------------------------------------------------------------- */

extern vibe_log_module_t _vibe_log_modules_start[];
extern vibe_log_module_t _vibe_log_modules_end[];

/* Weak defaults for host simulation / builds without a linker script */
__attribute__((weak)) vibe_log_module_t _vibe_log_modules_start[0];
__attribute__((weak)) vibe_log_module_t _vibe_log_modules_end[0];

/* -----------------------------------------------------------------------
 * Log message struct
 * --------------------------------------------------------------------- */

typedef struct {
    vibe_log_level_t  level;
    const char       *module_name;
    uint32_t          timestamp_ms;
    char              msg[128];
} vibe_log_msg_t;

/* -----------------------------------------------------------------------
 * Deferred ring buffer (only when CONFIG_LOG_DEFERRED)
 * --------------------------------------------------------------------- */

#ifdef CONFIG_LOG_DEFERRED

#ifndef CONFIG_LOG_BUFFER_SIZE
#define CONFIG_LOG_BUFFER_SIZE  1024
#endif

static vibe_log_msg_t g_log_ring[CONFIG_LOG_BUFFER_SIZE / sizeof(vibe_log_msg_t)];
static uint32_t       g_log_head = 0U;
static uint32_t       g_log_tail = 0U;
static vibe_spinlock_t g_log_lock = VIBE_SPINLOCK_INIT;

#endif /* CONFIG_LOG_DEFERRED */

/* -----------------------------------------------------------------------
 * Level name strings
 * --------------------------------------------------------------------- */

static const char * const _level_str[] = {
    [VIBE_LOG_LEVEL_ERR] = "ERR",
    [VIBE_LOG_LEVEL_WRN] = "WRN",
    [VIBE_LOG_LEVEL_INF] = "INF",
    [VIBE_LOG_LEVEL_DBG] = "DBG",
};

/* -----------------------------------------------------------------------
 * Backend dispatch (UART for now)
 * --------------------------------------------------------------------- */

static void _log_output(const vibe_log_module_t *mod,
                         vibe_log_level_t         level,
                         const char              *msg)
{
    /* Format: [LEVEL][module_name] msg\n */
    vibe_printk("[%s][%s] %s\n",
                _level_str[level],
                mod ? mod->name : "?",
                msg);
}

/* -----------------------------------------------------------------------
 * _vibe_log_emit() — called by VIBE_LOG_* macros
 * --------------------------------------------------------------------- */

void _vibe_log_emit(vibe_log_module_t *mod,
                    vibe_log_level_t   level,
                    const char        *file,
                    int                line,
                    const char        *fmt,
                    ...)
{
    (void)file; (void)line;

    if (mod == NULL) { return; }
    if (level > mod->level) { return; } /* Filter */

    /* Format the message */
    char msg[128];
    va_list args;
    va_start(args, fmt);
    /* Simple vsnprintf-like formatting using vibe_vprintk style */
    /* For now, just copy the format string since we don't have full vsnprintf */
    strncpy(msg, fmt, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';
    va_end(args);

#ifdef CONFIG_LOG_DEFERRED
    /* Write to ring buffer */
    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_log_lock);
    uint32_t next = (g_log_tail + 1) % ARRAY_SIZE(g_log_ring);
    if (next != g_log_head) {
        g_log_ring[g_log_tail].level       = level;
        g_log_ring[g_log_tail].module_name = mod->name;
        g_log_ring[g_log_tail].timestamp_ms = vibe_uptime_ms();
        strncpy(g_log_ring[g_log_tail].msg, msg, sizeof(g_log_ring[0].msg) - 1);
        g_log_tail = next;
    }
    vibe_spinlock_unlock_irqrestore(&g_log_lock, key);
#else
    /* Direct (synchronous) output */
    _log_output(mod, level, msg);
#endif
}

/* -----------------------------------------------------------------------
 * vibe_log_flush() — flush deferred buffer
 * --------------------------------------------------------------------- */

void vibe_log_flush(void)
{
#ifdef CONFIG_LOG_DEFERRED
    while (g_log_head != g_log_tail) {
        vibe_log_msg_t *m = &g_log_ring[g_log_head];
        vibe_printk("[%s][%s][%u] %s\n",
                    _level_str[m->level],
                    m->module_name,
                    (unsigned)m->timestamp_ms,
                    m->msg);
        g_log_head = (g_log_head + 1) % ARRAY_SIZE(g_log_ring);
    }
#endif
}

/* -----------------------------------------------------------------------
 * Runtime level control
 * --------------------------------------------------------------------- */

vibe_err_t vibe_log_set_level(const char *module_name, vibe_log_level_t level)
{
    for (vibe_log_module_t *m = _vibe_log_modules_start;
         m < _vibe_log_modules_end; m++) {
        if (strcmp(m->name, module_name) == 0) {
            m->level = level;
            return VIBE_OK;
        }
    }
    return VIBE_ENODEV;
}

vibe_log_level_t vibe_log_get_level(const char *module_name)
{
    for (vibe_log_module_t *m = _vibe_log_modules_start;
         m < _vibe_log_modules_end; m++) {
        if (strcmp(m->name, module_name) == 0) {
            return m->level;
        }
    }
    return VIBE_LOG_LEVEL_NONE;
}

/* -----------------------------------------------------------------------
 * _vibe_log_init()
 * --------------------------------------------------------------------- */

void _vibe_log_init(void)
{
    /* Set all modules to the compile-time default level */
    for (vibe_log_module_t *m = _vibe_log_modules_start;
         m < _vibe_log_modules_end; m++) {
        if (m->level > CONFIG_LOG_DEFAULT_LEVEL) {
            m->level = CONFIG_LOG_DEFAULT_LEVEL;
        }
    }
    vibe_printk("[log] logging subsystem initialised\n");
}

#else /* !CONFIG_LOG — provide empty stubs */

void _vibe_log_emit(vibe_log_module_t *mod, vibe_log_level_t level,
                    const char *file, int line, const char *fmt, ...) {}
void vibe_log_flush(void) {}
vibe_err_t vibe_log_set_level(const char *n, vibe_log_level_t l) { return VIBE_OK; }
vibe_log_level_t vibe_log_get_level(const char *n) { return VIBE_LOG_LEVEL_NONE; }
void _vibe_log_init(void) {}

#endif /* CONFIG_LOG */
