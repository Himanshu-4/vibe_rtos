#ifndef DRIVERS_TIMER_HW_H
#define DRIVERS_TIMER_HW_H

/**
 * @file drivers/timer_hw.h
 * @brief Hardware timer / PWM driver API for VibeRTOS.
 */

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hw_timer_ops {
    vibe_device_api_t base;
    vibe_err_t (*start)     (const vibe_device_t *dev, uint32_t period_us);
    vibe_err_t (*stop)      (const vibe_device_t *dev);
    uint32_t   (*read_us)   (const vibe_device_t *dev);
    vibe_err_t (*pwm_set)   (const vibe_device_t *dev, uint8_t channel,
                              uint32_t period_us, uint32_t pulse_us);
} hw_timer_ops_t;

static inline vibe_err_t pwm_set(const vibe_device_t *dev,
                                   uint8_t channel,
                                   uint32_t period_us,
                                   uint32_t pulse_us)
{
    return ((const hw_timer_ops_t *)dev->api)->pwm_set(dev, channel,
                                                        period_us, pulse_us);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_TIMER_HW_H */
