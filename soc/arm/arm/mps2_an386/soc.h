#ifndef MPS2_AN386_SOC_H
#define MPS2_AN386_SOC_H

/**
 * @file soc/arm/arm/mps2_an386/soc.h
 * @brief ARM MPS2 (AN386 FPGA image, Cortex-M4) memory map and peripherals.
 *
 * This "SoC" is the ARM MPS2 FPGA prototyping board loaded with the AN386
 * DesignStart Cortex-M4 image. It is the target VibeRTOS uses for QEMU
 * emulation (`qemu-system-arm -M mps2-an386`) because QEMU has no RP2040
 * machine model. Peripherals are from the ARM Cortex-M System Design Kit
 * (CMSDK).
 *
 * References:
 *   - ARM Application Note AN386 (DAI0386)
 *   - ARM MPS2 Technical Reference Manual (100112)
 *   - ARM Cortex-M System Design Kit TRM (DDI0479) — CMSDK APB UART
 */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Address space layout (matches QEMU's mps2-an386 machine model)
 * --------------------------------------------------------------------- */

#define MPS2_ZBT_SSRAM1_BASE    0x00000000U  /**< Code SRAM (acts as "flash") */
#define MPS2_ZBT_SSRAM1_SIZE    0x00400000U  /**< 4MB */

#define MPS2_ZBT_SSRAM23_BASE   0x20000000U  /**< Data SRAM */
#define MPS2_ZBT_SSRAM23_SIZE   0x00400000U  /**< 4MB */

#define MPS2_PSRAM_BASE         0x21000000U  /**< 16MB PSRAM (extra data RAM) */
#define MPS2_PSRAM_SIZE         0x01000000U

/* -----------------------------------------------------------------------
 * System clock
 * --------------------------------------------------------------------- */

#define MPS2_SYSCLK_HZ          25000000U    /**< 25 MHz main clock */

/* -----------------------------------------------------------------------
 * CMSDK APB peripherals
 * --------------------------------------------------------------------- */

#define MPS2_TIMER0_BASE        0x40000000U  /**< CMSDK APB timer 0 */
#define MPS2_TIMER1_BASE        0x40001000U  /**< CMSDK APB timer 1 */
#define MPS2_DUALTIMER_BASE     0x40002000U  /**< CMSDK dual timer */
#define MPS2_UART0_BASE         0x40004000U  /**< CMSDK APB UART 0 (console) */
#define MPS2_UART1_BASE         0x40005000U  /**< CMSDK APB UART 1 */
#define MPS2_UART2_BASE         0x40006000U  /**< CMSDK APB UART 2 */
#define MPS2_WATCHDOG_BASE      0x40008000U  /**< CMSDK watchdog */
#define MPS2_UART3_BASE         0x40200000U  /**< CMSDK APB UART 3 */
#define MPS2_UART4_BASE         0x40201000U  /**< CMSDK APB UART 4 */
#define MPS2_FPGAIO_BASE        0x40028000U  /**< FPGA IO block (LEDs, buttons) */
#define MPS2_SCC_BASE           0x4002F000U  /**< Serial config controller */

/* -----------------------------------------------------------------------
 * CMSDK APB UART register block (DDI0479, chapter "APB UART")
 * --------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t DATA;      /* 0x00 — TX (write) / RX (read) data */
    volatile uint32_t STATE;     /* 0x04 — buffer status flags */
    volatile uint32_t CTRL;      /* 0x08 — enable bits */
    volatile uint32_t INTCLEAR;  /* 0x0C — interrupt status / clear */
    volatile uint32_t BAUDDIV;   /* 0x10 — baud rate divider (min 16) */
} cmsdk_uart_t;

#define CMSDK_UART_STATE_TXFULL   (1U << 0)  /* TX buffer full */
#define CMSDK_UART_STATE_RXFULL   (1U << 1)  /* RX buffer full (data ready) */

#define CMSDK_UART_CTRL_TXEN      (1U << 0)
#define CMSDK_UART_CTRL_RXEN      (1U << 1)
#define CMSDK_UART_CTRL_TXIRQEN   (1U << 2)
#define CMSDK_UART_CTRL_RXIRQEN   (1U << 3)

#define MPS2_UART0  ((cmsdk_uart_t *)MPS2_UART0_BASE)

/* -----------------------------------------------------------------------
 * IRQ numbers (NVIC external interrupt lines, AN386)
 * --------------------------------------------------------------------- */

#define MPS2_IRQ_UART0_RX        0
#define MPS2_IRQ_UART0_TX        1
#define MPS2_IRQ_UART1_RX        2
#define MPS2_IRQ_UART1_TX        3
#define MPS2_IRQ_UART2_RX        4
#define MPS2_IRQ_UART2_TX        5
#define MPS2_IRQ_TIMER0          8
#define MPS2_IRQ_TIMER1          9
#define MPS2_IRQ_DUALTIMER       10
#define MPS2_IRQ_UART0_OVERFLOW  12
#define MPS2_IRQ_UART1_OVERFLOW  13

/* -----------------------------------------------------------------------
 * Emulation helpers (implemented in soc.c)
 * --------------------------------------------------------------------- */

/**
 * @brief Terminate the QEMU process via ARM semihosting SYS_EXIT.
 *
 * Requires QEMU to be started with `-semihosting`. QEMU's process exit
 * status is 0 when @p code == 0, non-zero otherwise. On real hardware
 * (no debugger attached) the BKPT instruction escalates to HardFault,
 * so only call this in emulation/test builds.
 *
 * @param code  0 for success, non-zero for failure.
 */
void vibe_emu_exit(int code) __attribute__((noreturn));

#endif /* MPS2_AN386_SOC_H */
