/**
 * @file soc/arm/arm/mps2_an386/soc.c
 * @brief ARM MPS2 AN386 (Cortex-M4) SoC support for QEMU emulation.
 *
 * Provides:
 *   - SystemInit()        — strong override of the weak stub in startup.S:
 *                           sets the core clock and brings up the console UART.
 *   - vibe_console_putc() — strong override of the weak no-op in kernel/init.c:
 *                           routes vibe_printk output to CMSDK UART0, which
 *                           QEMU connects to stdio.
 *   - vibe_emu_exit()     — terminates QEMU via ARM semihosting SYS_EXIT so
 *                           automated test runs can report pass/fail through
 *                           the process exit code.
 *
 * NOTE: this file must be linked into the application as a direct source
 * (see boards/arm/mps2_an386/CMakeLists.txt → VIBE_SOC_SOURCES). If it were
 * archived into a static library, the linker might satisfy SystemInit /
 * vibe_console_putc from their weak defaults and never pull this object in.
 */

#include "soc.h"
#include "vibe/arch.h"
#include "vibe/sys/printk.h"

/* -----------------------------------------------------------------------
 * Console UART (CMSDK APB UART0 — QEMU wires this to -serial / stdio)
 * --------------------------------------------------------------------- */

static void _uart0_init(void)
{
    /* BAUDDIV = clock / baud. QEMU ignores the actual value (bytes are
     * delivered instantly to the char backend) but the model requires
     * BAUDDIV >= 16 before it will transmit. Use the real 115200 divider
     * so the code is also correct on physical MPS2 hardware. */
    MPS2_UART0->BAUDDIV = MPS2_SYSCLK_HZ / 115200U;
    MPS2_UART0->CTRL    = CMSDK_UART_CTRL_TXEN | CMSDK_UART_CTRL_RXEN;
}

void vibe_console_putc(char c)
{
    while (MPS2_UART0->STATE & CMSDK_UART_STATE_TXFULL) {
        /* Wait for TX buffer space (never blocks in QEMU). */
    }
    MPS2_UART0->DATA = (uint32_t)(uint8_t)c;
}

/* -----------------------------------------------------------------------
 * SystemInit — called from Reset_Handler after .data/.bss initialisation
 * --------------------------------------------------------------------- */

void SystemInit(void)
{
    /* The AN386 image runs from a fixed 25 MHz clock — no PLL setup
     * needed. Tell the arch layer so SysTick reload values are right. */
    arch_set_core_clock(MPS2_SYSCLK_HZ);

    _uart0_init();
}

/* -----------------------------------------------------------------------
 * Semihosting exit — lets test scripts read pass/fail from QEMU's
 * exit status (requires `-semihosting` on the QEMU command line).
 * --------------------------------------------------------------------- */

#define SEMIHOST_SYS_EXIT                  0x18U
#define ADP_STOPPED_APPLICATION_EXIT       0x20026U
#define ADP_STOPPED_INTERNAL_ERROR         0x20024U

static void _semihost_call(uint32_t op, uint32_t param)
{
    register uint32_t r0 __asm__("r0") = op;
    register uint32_t r1 __asm__("r1") = param;

    __asm__ volatile("bkpt #0xAB"
                     : "+r"(r0)
                     : "r"(r1)
                     : "memory");
}

void vibe_emu_exit(int code)
{
    /* QEMU maps ADP_Stopped_ApplicationExit to process exit(0) and any
     * other reason code to exit(1). */
    uint32_t reason = (code == 0) ? ADP_STOPPED_APPLICATION_EXIT
                                  : ADP_STOPPED_INTERNAL_ERROR;

    _semihost_call(SEMIHOST_SYS_EXIT, reason);

    /* Not reached under QEMU. On real hardware BKPT faults; halt here. */
    for (;;) {
    }
}
