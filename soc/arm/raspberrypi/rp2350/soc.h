#ifndef RP2350_SOC_H
#define RP2350_SOC_H

/**
 * @file soc/arm/raspberrypi/rp2350/soc.h
 * @brief RP2350 peripheral base addresses and memory map.
 *
 * Reference: RP2350 Datasheet (https://datasheets.raspberrypi.com/rp2350/)
 *
 * Most peripheral base addresses are the same as RP2040.
 * Key differences:
 *  - SRAM: 520KB (10 x 64KB banks)
 *  - Cortex-M33 cores (instead of M0+)
 *  - Hardware FPU
 *  - TrustZone
 *  - SHA-256 accelerator
 *  - HSTX (High-Speed Transmit) peripheral
 */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Address space layout
 * --------------------------------------------------------------------- */

#define RP2350_FLASH_BASE       0x10000000U  /**< XIP flash (external QSPI) */
#define RP2350_FLASH_SIZE       0x00200000U  /**< 2MB */

#define RP2350_SRAM_BASE        0x20000000U  /**< On-chip SRAM (520KB) */
#define RP2350_SRAM_SIZE        0x00082000U  /**< 520KB */

/* -----------------------------------------------------------------------
 * APB peripherals (same base addresses as RP2040 where compatible)
 * --------------------------------------------------------------------- */

#define RP2350_CLOCKS_BASE      0x40010000U
#define RP2350_RESETS_BASE      0x40020000U
#define RP2350_IO_BANK0_BASE    0x40028000U
#define RP2350_PADS_BANK0_BASE  0x40038000U
#define RP2350_XOSC_BASE        0x40048000U
#define RP2350_PLL_SYS_BASE     0x40050000U
#define RP2350_PLL_USB_BASE     0x40058000U
#define RP2350_UART0_BASE       0x40070000U
#define RP2350_UART1_BASE       0x40078000U
#define RP2350_SPI0_BASE        0x40080000U
#define RP2350_SPI1_BASE        0x40088000U
#define RP2350_I2C0_BASE        0x40090000U
#define RP2350_I2C1_BASE        0x40098000U
#define RP2350_ADC_BASE         0x400A0000U
#define RP2350_PWM_BASE         0x400A8000U
#define RP2350_TIMER0_BASE      0x400B0000U
#define RP2350_TIMER1_BASE      0x400B8000U
#define RP2350_WATCHDOG_BASE    0x400D0000U
#define RP2350_SHA256_BASE      0x400F8000U
#define RP2350_HSTX_BASE        0x50600000U

/* -----------------------------------------------------------------------
 * SIO (Single-cycle I/O) — same as RP2040
 * --------------------------------------------------------------------- */

#define RP2350_SIO_BASE         0xD0000000U

#define RP2350_SIO_CPUID        (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x000))
#define RP2350_SIO_GPIO_IN      (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x004))
#define RP2350_SIO_GPIO_OUT_SET (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x014))
#define RP2350_SIO_GPIO_OUT_CLR (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x018))
#define RP2350_SIO_GPIO_OE_SET  (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x024))
#define RP2350_SIO_GPIO_OE_CLR  (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x028))

/* Inter-core FIFO */
#define RP2350_SIO_FIFO_ST      (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x050))
#define RP2350_SIO_FIFO_WR      (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x054))
#define RP2350_SIO_FIFO_RD      (*(volatile uint32_t *)(RP2350_SIO_BASE + 0x058))

#endif /* RP2350_SOC_H */
