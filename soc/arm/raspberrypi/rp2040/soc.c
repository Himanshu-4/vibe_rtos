/**
 * @file soc/arm/raspberrypi/rp2040/soc.c
 * @brief RP2040 SoC initialisation stub.
 *
 * USER: Implement rp2040_init() with full clock and PLL setup.
 * This function is called from SystemInit() in startup.S.
 */

#include "soc.h"
#include "vibe/sys/printk.h"

/**
 * @brief RP2040 system clock and peripheral initialisation.
 *
 * TODO (USER): Implement the following in order:
 *
 *  1. Enable XOSC (external 12 MHz crystal):
 *     - XOSC->CTRL = XOSC_CTRL_FREQ_RANGE_1_15MHZ;
 *     - XOSC->STARTUP = (12000 + 127) / 128;  // ~1ms startup
 *     - XOSC->CTRL |= XOSC_CTRL_ENABLE;
 *     - while (!(XOSC->STATUS & XOSC_STATUS_STABLE));
 *
 *  2. Configure PLL_SYS for 125 MHz (or 133 MHz):
 *     - Reference: XOSC 12MHz / REFDIV=1 = 12 MHz ref
 *     - VCO = ref * FBDIV = 12 * 125 = 1500 MHz (with POSTDIV1=6, POSTDIV2=2 -> 125 MHz)
 *     - Set PLL_SYS->CS, PRIM, PWR registers accordingly.
 *
 *  3. Switch clk_sys and clk_ref sources to XOSC / PLL_SYS.
 *
 *  4. Configure clk_peri (for UART, SPI, I2C) from clk_sys / 1.
 *
 *  5. Configure clk_adc from PLL_USB 48 MHz if ADC is needed.
 *
 *  6. Set flash XIP timing for higher clock speeds (set SSI clock divisor).
 *
 *  7. De-assert RESETS for all needed peripherals:
 *     RESETS->RESET_CLR = IO_BANK0 | PADS_BANK0 | UART0 | SPI0 | I2C0 | ADC | ...
 *     while (!(RESETS->RESET_DONE & <mask>));
 *
 * Reference: RP2040 Datasheet §4 (Clocks) and Raspberry Pi Pico SDK pico_stdlib.
 */
void rp2040_init(void)
{
    /*
     * Stub — system starts at ~6MHz ROSC (ring oscillator).
     * Replace with actual clock init above for reliable UART baud rates.
     */
    vibe_printk("[rp2040] WARNING: using default ROSC clock (~6MHz)\n");
    vibe_printk("[rp2040] TODO: implement rp2040_init() for PLL/XOSC clock setup\n");
}
