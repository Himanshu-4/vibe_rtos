#ifndef VIBE_SHELL_H
#define VIBE_SHELL_H

/**
 * @file subsys/shell/shell.h
 * @brief VibeRTOS interactive serial shell API.
 */

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_SHELL_MAX_ARGS
#define CONFIG_SHELL_MAX_ARGS  8
#endif

#ifndef CONFIG_SHELL_BUF_SIZE
#define CONFIG_SHELL_BUF_SIZE  128
#endif

#ifndef CONFIG_SHELL_PROMPT_STR
#define CONFIG_SHELL_PROMPT_STR  "vibe> "
#endif

/* -----------------------------------------------------------------------
 * Shell command descriptor
 * --------------------------------------------------------------------- */

/** Shell command handler: returns 0 on success, negative on error. */
typedef int (*vibe_shell_cmd_fn_t)(int argc, char *argv[]);

typedef struct vibe_shell_cmd {
    const char           *name;    /**< Command name (e.g., "reboot"). */
    const char           *help;    /**< One-line help string. */
    vibe_shell_cmd_fn_t   handler; /**< Handler function. */
} vibe_shell_cmd_t;

/* -----------------------------------------------------------------------
 * Command registration macro (linker section)
 * --------------------------------------------------------------------- */

/**
 * VIBE_SHELL_CMD_REGISTER — register a shell command at link time.
 *
 * @param cmd_name  C identifier for the command struct.
 * @param cmd_str   String name typed by the user (e.g., "reboot").
 * @param help_str  Short help text shown by the 'help' command.
 * @param fn        Handler function.
 */
#define VIBE_SHELL_CMD_REGISTER(cmd_name, cmd_str, help_str, fn)           \
    static const vibe_shell_cmd_t _shell_cmd_##cmd_name                     \
        __attribute__((used, section("._vibe_shell_cmds"))) = {             \
        .name    = (cmd_str),                                               \
        .help    = (help_str),                                              \
        .handler = (fn),                                                    \
    }

/* -----------------------------------------------------------------------
 * Shell API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise the shell subsystem.
 *
 * @param uart_dev  UART device for input/output (from vibe_device_get()).
 * @return          VIBE_OK or error code.
 */
vibe_err_t vibe_shell_init(const vibe_device_t *uart_dev);

/**
 * @brief Process pending shell input (call from shell thread or main loop).
 *
 * Reads characters from the UART, echoes them, and dispatches completed
 * command lines to the registered command handlers.
 */
void vibe_shell_process(void);

/**
 * @brief Print a string to the shell output.
 *
 * @param str  Null-terminated string to print.
 */
void vibe_shell_print(const char *str);

/**
 * @brief Printf-style output to the shell.
 */
void vibe_shell_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/* -----------------------------------------------------------------------
 * Linker section symbols for command iteration
 * --------------------------------------------------------------------- */

extern const vibe_shell_cmd_t _vibe_shell_cmds_start[];
extern const vibe_shell_cmd_t _vibe_shell_cmds_end[];

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SHELL_H */
