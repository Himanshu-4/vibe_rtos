/**
 * @file drivers/timer/timer.c
 * @brief Hardware timer/PWM driver stub for VibeRTOS.
 * USER: Implement using RP2040 PWM slices (base 0x40050000) or RP2350 equivalent.
 */

#include "drivers/timer_hw.h"
#include "vibe/device.h"

#ifdef CONFIG_TIMER_HW

static vibe_err_t _htimer_start(const vibe_device_t *dev, uint32_t period_us)
{ (void)dev; (void)period_us; return VIBE_ENOSYS; }

static vibe_err_t _htimer_stop(const vibe_device_t *dev)
{ (void)dev; return VIBE_ENOSYS; }

static uint32_t _htimer_read_us(const vibe_device_t *dev)
{ (void)dev; return 0U; }

static vibe_err_t _htimer_pwm_set(const vibe_device_t *dev,
                                    uint8_t channel,
                                    uint32_t period_us, uint32_t pulse_us)
{ (void)dev; (void)channel; (void)period_us; (void)pulse_us; return VIBE_ENOSYS; }

static vibe_err_t _htimer_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const hw_timer_ops_t _htimer0_ops = {
    .base     = { .type = "timer" },
    .start    = _htimer_start,
    .stop     = _htimer_stop,
    .read_us  = _htimer_read_us,
    .pwm_set  = _htimer_pwm_set,
};

VIBE_DEVICE_DEFINE(timer0, _htimer_init, NULL, NULL,
                   &_htimer0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_TIMER_HW */
