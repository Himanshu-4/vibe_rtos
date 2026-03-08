/**
 * @file drivers/gpio/gpio.c
 * @brief GPIO driver stub for VibeRTOS.
 *
 * USER: Replace the stub bodies with your hardware register access.
 * For RP2040, see the datasheet section on SIO and IO_BANK0.
 * For RP2350, same registers apply (Cortex-M33 variant).
 */

#include "drivers/gpio.h"
#include "vibe/device.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"

#ifdef CONFIG_GPIO

/* -----------------------------------------------------------------------
 * Driver state (per-pin callback registry)
 * --------------------------------------------------------------------- */

#define GPIO_MAX_PINS  32

typedef struct {
    gpio_irq_callback_t cb;
    void               *arg;
} gpio_pin_ctx_t;

typedef struct {
    gpio_pin_ctx_t pins[GPIO_MAX_PINS];
} gpio_data_t;

/* -----------------------------------------------------------------------
 * Ops implementations (stubs)
 * --------------------------------------------------------------------- */

static vibe_err_t _gpio_configure(const vibe_device_t *dev,
                                    gpio_pin_t pin,
                                    gpio_dir_t dir,
                                    gpio_pull_t pull)
{
    (void)dev;
    if (pin >= GPIO_MAX_PINS) { return VIBE_EINVAL; }

    /*
     * TODO (USER): Configure pin direction and pull.
     *
     * RP2040 example:
     *   uint32_t mask = 1U << pin;
     *   // Set function to SIO
     *   IO_BANK0->GPIO[pin].CTRL = 5;   // GPIO function = SIO
     *   // Set direction
     *   if (dir == GPIO_DIR_OUTPUT) SIO->GPIO_OE_SET = mask;
     *   else                        SIO->GPIO_OE_CLR = mask;
     *   // Set pull
     *   uint32_t pads = PADS_BANK0->GPIO[pin];
     *   pads &= ~(PADS_PUE | PADS_PDE);
     *   if (pull == GPIO_PULL_UP)   pads |= PADS_PUE;
     *   if (pull == GPIO_PULL_DOWN) pads |= PADS_PDE;
     *   PADS_BANK0->GPIO[pin] = pads;
     */
    vibe_printk("[gpio] configure pin %u dir=%d pull=%d\n",
                (unsigned)pin, dir, pull);
    return VIBE_OK;
}

static int _gpio_read(const vibe_device_t *dev, gpio_pin_t pin)
{
    (void)dev;
    if (pin >= GPIO_MAX_PINS) { return -1; }
    /*
     * TODO (USER): Read pin level.
     * RP2040: return (SIO->GPIO_IN >> pin) & 1U;
     */
    return 0;
}

static vibe_err_t _gpio_write(const vibe_device_t *dev,
                               gpio_pin_t pin, int val)
{
    (void)dev;
    if (pin >= GPIO_MAX_PINS) { return VIBE_EINVAL; }
    /*
     * TODO (USER): Write pin level.
     * RP2040:
     *   if (val) SIO->GPIO_OUT_SET = 1U << pin;
     *   else     SIO->GPIO_OUT_CLR = 1U << pin;
     */
    return VIBE_OK;
}

static vibe_err_t _gpio_toggle(const vibe_device_t *dev, gpio_pin_t pin)
{
    (void)dev;
    if (pin >= GPIO_MAX_PINS) { return VIBE_EINVAL; }
    /*
     * TODO (USER): Toggle pin.
     * RP2040: SIO->GPIO_OUT_XOR = 1U << pin;
     */
    return VIBE_OK;
}

static vibe_err_t _gpio_irq_configure(const vibe_device_t *dev,
                                        gpio_pin_t pin,
                                        gpio_irq_mode_t mode,
                                        gpio_irq_callback_t cb,
                                        void *arg)
{
    gpio_data_t *data = (gpio_data_t *)dev->data;
    if (pin >= GPIO_MAX_PINS) { return VIBE_EINVAL; }

    data->pins[pin].cb  = cb;
    data->pins[pin].arg = arg;

    /*
     * TODO (USER): Configure IO_BANK0 interrupt for this pin and mode.
     * Enable the GPIO IRQ in NVIC.
     */
    return VIBE_OK;
}

static vibe_err_t _gpio_irq_enable(const vibe_device_t *dev, gpio_pin_t pin)
{
    (void)dev; (void)pin;
    /* TODO: NVIC enable GPIO IRQ */
    return VIBE_OK;
}

static vibe_err_t _gpio_irq_disable(const vibe_device_t *dev, gpio_pin_t pin)
{
    (void)dev; (void)pin;
    /* TODO: NVIC disable GPIO IRQ */
    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Device init
 * --------------------------------------------------------------------- */

static vibe_err_t _gpio_init(const vibe_device_t *dev)
{
    (void)dev;
    vibe_printk("[gpio] driver initialised\n");
    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Driver registration
 * --------------------------------------------------------------------- */

static gpio_data_t _gpio0_data;

static const gpio_ops_t _gpio0_ops = {
    .base         = { .type = "gpio" },
    .configure    = _gpio_configure,
    .read         = _gpio_read,
    .write        = _gpio_write,
    .toggle       = _gpio_toggle,
    .irq_configure = _gpio_irq_configure,
    .irq_enable   = _gpio_irq_enable,
    .irq_disable  = _gpio_irq_disable,
};

VIBE_DEVICE_DEFINE(gpio0, _gpio_init, NULL, &_gpio0_data,
                   &_gpio0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_GPIO */
