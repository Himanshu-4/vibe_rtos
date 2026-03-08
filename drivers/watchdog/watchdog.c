/**
 * @file drivers/watchdog/watchdog.c
 * @brief Watchdog timer driver stub for VibeRTOS.
 * USER: implement using RP2040 Watchdog peripheral (base 0x40058000).
 *
 * RP2040 Watchdog:
 *   CTRL   0x00 — control (bit 31 = enable, bit 30 = pause on debug)
 *   LOAD   0x04 — countdown load value (24-bit, 1us per tick)
 *   REASON 0x08 — reset reason (bit 0 = timer, bit 1 = force)
 *   SCRATCH[8]  — scratch registers
 *   TICK   0x2C — tick generator (for 1us resolution from 12MHz xosc)
 */

#include "drivers/watchdog.h"
#include "vibe/device.h"

#ifdef CONFIG_WATCHDOG

static vibe_err_t _wdt_setup(const vibe_device_t *dev, const wdt_config_t *cfg)
{
    (void)dev;
    /*
     * TODO (USER):
     *   // Load timeout (microseconds = timeout_ms * 1000)
     *   WDT->LOAD = cfg->timeout_ms * 1000U;
     *   // Enable, optionally pause on debug
     *   uint32_t ctrl = WDT_CTRL_ENABLE;
     *   if (cfg->pause_on_debug) ctrl |= WDT_CTRL_PAUSE_DBG;
     *   WDT->CTRL = ctrl;
     */
    (void)cfg;
    return VIBE_ENOSYS;
}

static vibe_err_t _wdt_feed(const vibe_device_t *dev)
{
    (void)dev;
    /* TODO: WDT->LOAD = <same value as setup> to kick the watchdog */
    return VIBE_ENOSYS;
}

static vibe_err_t _wdt_disable(const vibe_device_t *dev)
{
    (void)dev;
    /* TODO: WDT->CTRL &= ~WDT_CTRL_ENABLE; (note: may not be possible on all SoCs) */
    return VIBE_ENOSYS;
}

static vibe_err_t _wdt_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const wdt_ops_t _wdt0_ops = {
    .base    = { .type = "watchdog" },
    .setup   = _wdt_setup,
    .feed    = _wdt_feed,
    .disable = _wdt_disable,
};

VIBE_DEVICE_DEFINE(watchdog0, _wdt_init, NULL, NULL,
                   &_wdt0_ops, VIBE_INIT_PRE_KERNEL_1, NULL);

#endif /* CONFIG_WATCHDOG */
