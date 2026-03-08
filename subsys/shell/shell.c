/**
 * @file subsys/shell/shell.c
 * @brief VibeRTOS interactive serial shell implementation.
 *
 * Reads characters from a UART device, echoes them, and dispatches
 * completed command lines to registered command handlers.
 *
 * Built-in commands: help, thread_list, mem_stats, reboot.
 * User commands registered with VIBE_SHELL_CMD_REGISTER().
 */

#include "shell.h"
#include "vibe/kernel.h"
#include "vibe/thread.h"
#include "drivers/uart.h"
#include <string.h>
#include <stdarg.h>

#ifdef CONFIG_SHELL

/* -----------------------------------------------------------------------
 * Shell state
 * --------------------------------------------------------------------- */

static const vibe_device_t *g_shell_uart = NULL;
static char                 g_line_buf[CONFIG_SHELL_BUF_SIZE];
static int                  g_line_len = 0;

/* -----------------------------------------------------------------------
 * Weak linker section symbols (overridden by linker script in real builds)
 * --------------------------------------------------------------------- */

__attribute__((weak)) const vibe_shell_cmd_t _vibe_shell_cmds_start[0];
__attribute__((weak)) const vibe_shell_cmd_t _vibe_shell_cmds_end[0];

/* -----------------------------------------------------------------------
 * Output helpers
 * --------------------------------------------------------------------- */

void vibe_shell_print(const char *str)
{
    if (g_shell_uart == NULL || str == NULL) { return; }
    uart_write(g_shell_uart, (const uint8_t *)str, strlen(str));
}

void vibe_shell_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    /* Minimal vsnprintf — use vibe_vprintk style */
    vibe_vprintk(fmt, args); /* Falls through to uart via _printk_putchar */
    va_end(args);
    (void)buf; /* buf unused in stub */
}

/* -----------------------------------------------------------------------
 * Built-in commands
 * --------------------------------------------------------------------- */

static int _cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vibe_shell_print("VibeRTOS Shell — available commands:\r\n");

    const vibe_shell_cmd_t *cmd;
    for (cmd = _vibe_shell_cmds_start; cmd < _vibe_shell_cmds_end; cmd++) {
        vibe_shell_print("  ");
        vibe_shell_print(cmd->name);
        vibe_shell_print("\t— ");
        vibe_shell_print(cmd->help);
        vibe_shell_print("\r\n");
    }
    return 0;
}

static int _cmd_thread_list(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vibe_shell_print("Thread list:\r\n");
    /* TODO: iterate global thread list when implemented */
    vibe_thread_t *self = vibe_thread_self();
    if (self) {
        vibe_shell_print("  [current] ");
        vibe_shell_print(vibe_thread_get_name(self));
        vibe_shell_print("\r\n");
    }
    return 0;
}

static int _cmd_mem_stats(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vibe_shell_print("Memory stats:\r\n");
#ifdef CONFIG_HEAP_MEM
    size_t used, free;
    vibe_heap_stats(&used, &free);
    /* Simple integer print */
    vibe_printk("  heap: used=%u free=%u\r\n", (unsigned)used, (unsigned)free);
#else
    vibe_shell_print("  heap: disabled\r\n");
#endif
    return 0;
}

static int _cmd_reboot(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vibe_shell_print("Rebooting...\r\n");
    vibe_reboot();
    return 0;
}

static int _cmd_uptime(int argc, char *argv[])
{
    (void)argc; (void)argv;
    uint32_t ms = vibe_uptime_ms();
    vibe_printk("Uptime: %u ms\r\n", ms);
    return 0;
}

/* Register built-in commands */
VIBE_SHELL_CMD_REGISTER(help,        "help",        "Show this help",          _cmd_help);
VIBE_SHELL_CMD_REGISTER(thread_list, "thread_list", "List all threads",        _cmd_thread_list);
VIBE_SHELL_CMD_REGISTER(mem_stats,   "mem_stats",   "Show memory statistics",  _cmd_mem_stats);
VIBE_SHELL_CMD_REGISTER(reboot,      "reboot",      "Reboot the system",       _cmd_reboot);
VIBE_SHELL_CMD_REGISTER(uptime,      "uptime",      "Show system uptime",      _cmd_uptime);

/* -----------------------------------------------------------------------
 * Line parser and command dispatch
 * --------------------------------------------------------------------- */

static void _shell_dispatch(char *line)
{
    /* Split into argv */
    char *argv[CONFIG_SHELL_MAX_ARGS];
    int   argc = 0;
    char *tok  = line;
    char *p    = line;

    while (*p && argc < CONFIG_SHELL_MAX_ARGS - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') { p++; }
        if (*p == '\0') { break; }
        argv[argc++] = p;
        /* Scan to end of token */
        while (*p && *p != ' ' && *p != '\t') { p++; }
        if (*p) { *p++ = '\0'; }
    }
    argv[argc] = NULL;

    if (argc == 0) { return; }

    (void)tok;

    /* Look up the command */
    const vibe_shell_cmd_t *cmd;
    for (cmd = _vibe_shell_cmds_start; cmd < _vibe_shell_cmds_end; cmd++) {
        if (strcmp(cmd->name, argv[0]) == 0) {
            int ret = cmd->handler(argc, argv);
            if (ret != 0) {
                vibe_printk("command '%s' returned %d\r\n", argv[0], ret);
            }
            return;
        }
    }

    vibe_shell_print("Unknown command: '");
    vibe_shell_print(argv[0]);
    vibe_shell_print("'. Type 'help' for available commands.\r\n");
}

/* -----------------------------------------------------------------------
 * vibe_shell_init()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_shell_init(const vibe_device_t *uart_dev)
{
    g_shell_uart = uart_dev;
    g_line_len   = 0;
    memset(g_line_buf, 0, sizeof(g_line_buf));

    vibe_shell_print("\r\n");
    vibe_shell_print("VibeRTOS v" VIBE_RTOS_VERSION_STRING "\r\n");
    vibe_shell_print(CONFIG_SHELL_PROMPT_STR);

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * vibe_shell_process() — call periodically or from shell thread
 * --------------------------------------------------------------------- */

void vibe_shell_process(void)
{
    if (g_shell_uart == NULL) { return; }

    uint8_t c;
    while (uart_getc(g_shell_uart, &c) == VIBE_OK) {
        if (c == '\r' || c == '\n') {
            /* Echo newline */
            vibe_shell_print("\r\n");

            if (g_line_len > 0) {
                g_line_buf[g_line_len] = '\0';
                _shell_dispatch(g_line_buf);
                g_line_len = 0;
            }

            vibe_shell_print(CONFIG_SHELL_PROMPT_STR);

        } else if (c == 0x08 || c == 0x7F) {
            /* Backspace */
            if (g_line_len > 0) {
                g_line_len--;
                vibe_shell_print("\b \b");
            }

        } else if (c >= 0x20 && g_line_len < CONFIG_SHELL_BUF_SIZE - 1) {
            /* Printable character — echo and append */
            g_line_buf[g_line_len++] = (char)c;
            uint8_t echo = c;
            uart_write(g_shell_uart, &echo, 1);
        }
    }
}

#endif /* CONFIG_SHELL */
