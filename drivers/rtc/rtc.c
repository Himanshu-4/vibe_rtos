/**
 * @file drivers/rtc/rtc.c
 * @brief RTC driver stub for VibeRTOS.
 * USER: implement using RP2040 RTC peripheral (base 0x4005C000).
 */

#include "drivers/rtc.h"
#include "vibe/device.h"

#ifdef CONFIG_RTC

static vibe_err_t _rtc_set_time(const vibe_device_t *dev,
                                  const vibe_rtc_time_t *t)
{ (void)dev; (void)t; return VIBE_ENOSYS; }

static vibe_err_t _rtc_get_time(const vibe_device_t *dev, vibe_rtc_time_t *t)
{ (void)dev; (void)t; return VIBE_ENOSYS; }

static vibe_err_t _rtc_set_alarm(const vibe_device_t *dev,
                                   const vibe_rtc_time_t *t,
                                   rtc_alarm_cb_t cb, void *arg)
{ (void)dev; (void)t; (void)cb; (void)arg; return VIBE_ENOSYS; }

static vibe_err_t _rtc_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const rtc_ops_t _rtc0_ops = {
    .base      = { .type = "rtc" },
    .set_time  = _rtc_set_time,
    .get_time  = _rtc_get_time,
    .set_alarm = _rtc_set_alarm,
};

VIBE_DEVICE_DEFINE(rtc0, _rtc_init, NULL, NULL,
                   &_rtc0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_RTC */
