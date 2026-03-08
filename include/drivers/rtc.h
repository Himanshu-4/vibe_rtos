#ifndef DRIVERS_RTC_H
#define DRIVERS_RTC_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t  month;   /**< 1-12 */
    uint8_t  day;     /**< 1-31 */
    uint8_t  hour;    /**< 0-23 */
    uint8_t  minute;  /**< 0-59 */
    uint8_t  second;  /**< 0-59 */
    uint8_t  dotw;    /**< Day of week: 0=Sunday */
} vibe_rtc_time_t;

typedef void (*rtc_alarm_cb_t)(void *arg);

typedef struct rtc_ops {
    vibe_device_api_t base;
    vibe_err_t (*set_time)  (const vibe_device_t *dev, const vibe_rtc_time_t *t);
    vibe_err_t (*get_time)  (const vibe_device_t *dev, vibe_rtc_time_t *t);
    vibe_err_t (*set_alarm) (const vibe_device_t *dev, const vibe_rtc_time_t *t,
                              rtc_alarm_cb_t cb, void *arg);
} rtc_ops_t;

static inline vibe_err_t rtc_set_time(const vibe_device_t *dev,
                                        const vibe_rtc_time_t *t)
{ return ((const rtc_ops_t *)dev->api)->set_time(dev, t); }

static inline vibe_err_t rtc_get_time(const vibe_device_t *dev,
                                        vibe_rtc_time_t *t)
{ return ((const rtc_ops_t *)dev->api)->get_time(dev, t); }

static inline vibe_err_t rtc_set_alarm(const vibe_device_t *dev,
                                         const vibe_rtc_time_t *t,
                                         rtc_alarm_cb_t cb, void *arg)
{ return ((const rtc_ops_t *)dev->api)->set_alarm(dev, t, cb, arg); }

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_RTC_H */
