#ifndef DRIVERS_WATCHDOG_H
#define DRIVERS_WATCHDOG_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t timeout_ms;  /**< Watchdog timeout in milliseconds. */
    bool     pause_on_debug; /**< Pause counter when debugger halts CPU. */
} wdt_config_t;

typedef struct wdt_ops {
    vibe_device_api_t base;
    vibe_err_t (*setup)  (const vibe_device_t *dev, const wdt_config_t *cfg);
    vibe_err_t (*feed)   (const vibe_device_t *dev);
    vibe_err_t (*disable)(const vibe_device_t *dev);
} wdt_ops_t;

static inline vibe_err_t wdt_setup(const vibe_device_t *dev,
                                     const wdt_config_t *cfg)
{ return ((const wdt_ops_t *)dev->api)->setup(dev, cfg); }

static inline vibe_err_t wdt_feed(const vibe_device_t *dev)
{ return ((const wdt_ops_t *)dev->api)->feed(dev); }

static inline vibe_err_t wdt_disable(const vibe_device_t *dev)
{ return ((const wdt_ops_t *)dev->api)->disable(dev); }

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_WATCHDOG_H */
