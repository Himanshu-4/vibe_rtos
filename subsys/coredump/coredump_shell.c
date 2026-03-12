/**
 * @file subsys/coredump/coredump_shell.c
 * @brief Shell command: "coredump" — inspect or clear the stored coredump.
 *
 * Commands:
 *   coredump          — print full coredump (same as vibe_coredump_dump())
 *   coredump clear    — erase the stored record
 *   coredump status   — one-line valid/invalid notice
 */

#include "vibe/coredump.h"
#include "vibe/sys/printk.h"

#if defined(CONFIG_COREDUMP) && defined(CONFIG_COREDUMP_SHELL)

#include "../shell/shell.h"

static int _cmd_coredump(int argc, char *argv[])
{
    if (argc >= 2) {
        if (__builtin_strcmp(argv[1], "clear") == 0) {
            vibe_coredump_clear();
            vibe_printk("coredump: record cleared.\n");
            return 0;
        }
        if (__builtin_strcmp(argv[1], "status") == 0) {
            if (vibe_coredump_is_valid()) {
                const vibe_coredump_t *cd = vibe_coredump_get();
                vibe_printk("coredump: valid (reason=0x%x, thread='%s')\n",
                            (unsigned)cd->reason, cd->thread_name);
            } else {
                vibe_printk("coredump: no valid record.\n");
            }
            return 0;
        }
        vibe_printk("Usage: coredump [clear|status]\n");
        return -1;
    }

    /* Default: dump full record */
    if (!vibe_coredump_is_valid()) {
        vibe_printk("coredump: no valid record.\n");
        return 0;
    }
    vibe_coredump_dump();
    return 0;
}

VIBE_SHELL_CMD_REGISTER(coredump, "coredump",
                        "Print/clear the stored crash coredump",
                        _cmd_coredump);

#endif /* CONFIG_COREDUMP && CONFIG_COREDUMP_SHELL */
