#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

/**
 * @file drivers/gpio.h
 * @brief GPIO driver API for VibeRTOS.
 */

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * GPIO types
 * --------------------------------------------------------------------- */

typedef uint32_t gpio_pin_t;

typedef enum {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1,
} gpio_dir_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP   = 1,
    GPIO_PULL_DOWN = 2,
} gpio_pull_t;

typedef enum {
    GPIO_IRQ_RISING_EDGE  = 0,
    GPIO_IRQ_FALLING_EDGE = 1,
    GPIO_IRQ_BOTH_EDGES   = 2,
    GPIO_IRQ_LEVEL_HIGH   = 3,
    GPIO_IRQ_LEVEL_LOW    = 4,
} gpio_irq_mode_t;

typedef void (*gpio_irq_callback_t)(gpio_pin_t pin, void *arg);

/* -----------------------------------------------------------------------
 * GPIO driver ops
 * --------------------------------------------------------------------- */

typedef struct gpio_ops {
    vibe_device_api_t base;  /**< Must be first — common device API. */
    vibe_err_t (*configure)(const vibe_device_t *dev, gpio_pin_t pin,
                             gpio_dir_t dir, gpio_pull_t pull);
    int        (*read)     (const vibe_device_t *dev, gpio_pin_t pin);
    vibe_err_t (*write)    (const vibe_device_t *dev, gpio_pin_t pin, int val);
    vibe_err_t (*toggle)   (const vibe_device_t *dev, gpio_pin_t pin);
    vibe_err_t (*irq_configure)(const vibe_device_t *dev, gpio_pin_t pin,
                                 gpio_irq_mode_t mode,
                                 gpio_irq_callback_t cb, void *arg);
    vibe_err_t (*irq_enable) (const vibe_device_t *dev, gpio_pin_t pin);
    vibe_err_t (*irq_disable)(const vibe_device_t *dev, gpio_pin_t pin);
} gpio_ops_t;

/* -----------------------------------------------------------------------
 * GPIO API
 * --------------------------------------------------------------------- */

static inline vibe_err_t gpio_configure(const vibe_device_t *dev,
                                          gpio_pin_t pin,
                                          gpio_dir_t dir,
                                          gpio_pull_t pull)
{
    const gpio_ops_t *ops = (const gpio_ops_t *)dev->api;
    return ops->configure(dev, pin, dir, pull);
}

static inline int gpio_read(const vibe_device_t *dev, gpio_pin_t pin)
{
    const gpio_ops_t *ops = (const gpio_ops_t *)dev->api;
    return ops->read(dev, pin);
}

static inline vibe_err_t gpio_write(const vibe_device_t *dev,
                                     gpio_pin_t pin, int val)
{
    const gpio_ops_t *ops = (const gpio_ops_t *)dev->api;
    return ops->write(dev, pin, val);
}

static inline vibe_err_t gpio_toggle(const vibe_device_t *dev, gpio_pin_t pin)
{
    const gpio_ops_t *ops = (const gpio_ops_t *)dev->api;
    return ops->toggle(dev, pin);
}

static inline vibe_err_t gpio_irq_configure(const vibe_device_t *dev,
                                              gpio_pin_t pin,
                                              gpio_irq_mode_t mode,
                                              gpio_irq_callback_t cb,
                                              void *arg)
{
    const gpio_ops_t *ops = (const gpio_ops_t *)dev->api;
    return ops->irq_configure(dev, pin, mode, cb, arg);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_GPIO_H */
