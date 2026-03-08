/**
 * @file soc/arm/raspberrypi/rp2350/soc.c
 * @brief RP2350 SoC initialisation stub.
 *
 * USER: Implement rp2350_init() with full clock and PLL setup.
 * The RP2350 clock tree is similar to RP2040 but with additional options.
 *
 * Key differences from RP2040:
 *  - 150 MHz default system clock target
 *  - Supports HF XOSC (12 or 48 MHz crystal)
 *  - USB PLL and SYS PLL configurations differ
 *  - Additional POWMAN (power management) peripheral
 */

#include "soc.h"
#include "vibe/sys/printk.h"

void rp2350_init(void)
{
    /*
     * TODO (USER): RP2350 clock initialisation.
     *
     * Suggested sequence:
     *  1. Start XOSC (12 MHz crystal).
     *  2. Configure PLL_SYS for 150 MHz:
     *     - VCO = 12MHz * 125 = 1500 MHz
     *     - POSTDIV1 = 5, POSTDIV2 = 2 -> 150 MHz
     *  3. Switch clk_sys to PLL_SYS.
     *  4. Configure clk_peri = clk_sys / 1 = 150 MHz.
     *  5. Configure USB PLL for 48 MHz (if USB needed).
     *  6. De-assert RESETS for all needed peripherals.
     *  7. (Optional) Configure TrustZone SAU regions if security needed.
     */
    vibe_printk("[rp2350] WARNING: using default boot clock\n");
    vibe_printk("[rp2350] TODO: implement rp2350_init() for PLL/XOSC setup\n");
}
