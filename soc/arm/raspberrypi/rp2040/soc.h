#ifndef RP2040_SOC_H
#define RP2040_SOC_H

/**
 * @file soc/arm/raspberrypi/rp2040/soc.h
 * @brief RP2040 peripheral base addresses and memory map.
 *
 * Reference: RP2040 Datasheet (https://datasheets.raspberrypi.com/rp2040/)
 *
 * USER: Extend with full register structs for each peripheral as needed.
 */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Address space layout
 * --------------------------------------------------------------------- */

#define RP2040_FLASH_BASE       0x10000000U  /**< XIP (execute-in-place) flash */
#define RP2040_FLASH_SIZE       0x00200000U  /**< 2MB (external QSPI) */

#define RP2040_SRAM_BASE        0x20000000U  /**< On-chip SRAM (264KB, 6 banks) */
#define RP2040_SRAM_SIZE        0x00042000U  /**< 264KB */

#define RP2040_SRAM_STRIPED_BASE 0x20000000U /**< Striped SRAM banks 0-3 */
#define RP2040_SRAM4_BASE       0x20040000U  /**< SRAM bank 4 (4KB) */
#define RP2040_SRAM5_BASE       0x20041000U  /**< SRAM bank 5 (4KB) */

/* -----------------------------------------------------------------------
 * APB peripherals (0x40000000 - 0x4003FFFF)
 * --------------------------------------------------------------------- */

#define RP2040_SYSCFG_BASE      0x40004000U
#define RP2040_CLOCKS_BASE      0x40008000U
#define RP2040_RESETS_BASE      0x4000C000U
#define RP2040_PSM_BASE         0x40010000U
#define RP2040_IO_BANK0_BASE    0x40014000U
#define RP2040_IO_QSPI_BASE     0x40018000U
#define RP2040_PADS_BANK0_BASE  0x4001C000U
#define RP2040_PADS_QSPI_BASE   0x40020000U
#define RP2040_XOSC_BASE        0x40024000U
#define RP2040_PLL_SYS_BASE     0x40028000U
#define RP2040_PLL_USB_BASE     0x4002C000U
#define RP2040_BUSCTRL_BASE     0x40030000U
#define RP2040_UART0_BASE       0x40034000U
#define RP2040_UART1_BASE       0x40038000U
#define RP2040_SPI0_BASE        0x4003C000U
#define RP2040_SPI1_BASE        0x40040000U
#define RP2040_I2C0_BASE        0x40044000U
#define RP2040_I2C1_BASE        0x40048000U
#define RP2040_ADC_BASE         0x4004C000U
#define RP2040_PWM_BASE         0x40050000U
#define RP2040_TIMER_BASE       0x40054000U
#define RP2040_WATCHDOG_BASE    0x40058000U
#define RP2040_RTC_BASE         0x4005C000U
#define RP2040_ROSC_BASE        0x40060000U
#define RP2040_VREG_CHIP_RESET_BASE 0x40064000U
#define RP2040_TBMAN_BASE       0x4006C000U

/* -----------------------------------------------------------------------
 * AHB peripherals
 * --------------------------------------------------------------------- */

#define RP2040_DMA_BASE         0x50000000U
#define RP2040_USBCTRL_BASE     0x50100000U
#define RP2040_PIO0_BASE        0x50200000U
#define RP2040_PIO1_BASE        0x50300000U
#define RP2040_XIP_SSI_BASE     0x18000000U

/* -----------------------------------------------------------------------
 * SIO — Single-cycle I/O (direct core access, no bus fabric)
 * --------------------------------------------------------------------- */

#define RP2040_SIO_BASE         0xD0000000U

#define RP2040_SIO_CPUID        (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x000))
#define RP2040_SIO_GPIO_IN      (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x004))
#define RP2040_SIO_GPIO_OE      (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x020))
#define RP2040_SIO_GPIO_OE_SET  (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x024))
#define RP2040_SIO_GPIO_OE_CLR  (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x028))
#define RP2040_SIO_GPIO_OE_XOR  (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x02C))
#define RP2040_SIO_GPIO_OUT     (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x010))
#define RP2040_SIO_GPIO_OUT_SET (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x014))
#define RP2040_SIO_GPIO_OUT_CLR (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x018))
#define RP2040_SIO_GPIO_OUT_XOR (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x01C))

/* Inter-core FIFO */
#define RP2040_SIO_FIFO_ST      (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x050))
#define RP2040_SIO_FIFO_WR      (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x054))
#define RP2040_SIO_FIFO_RD      (*(volatile uint32_t *)(RP2040_SIO_BASE + 0x058))

/* -----------------------------------------------------------------------
 * Cortex-M0+ NVIC / SCB (standard ARM addresses)
 * --------------------------------------------------------------------- */

#define RP2040_NVIC_BASE        0xE000E100U
#define RP2040_SCB_BASE         0xE000ED00U
#define RP2040_SYSTICK_BASE     0xE000E010U

#endif /* RP2040_SOC_H */
