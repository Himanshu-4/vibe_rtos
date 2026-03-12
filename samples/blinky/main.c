/**
 * @file samples/blinky/main.c
 * @brief VibeRTOS blinky LED example.
 *
 * Demonstrates:
 *  - GPIO driver usage (gpio_configure, gpio_write, gpio_toggle)
 *  - Periodic thread with vibe_thread_sleep
 *  - Device lookup with vibe_device_get
 *
 * On Raspberry Pi Pico: LED is on GPIO 25.
 */

#include <vibe/kernel.h>
#include <drivers/gpio.h>

#define LED_PIN   25U      /**< GPIO 25 = onboard LED on Pico */
#define BLINK_MS  500U     /**< Toggle every 500ms */

/* -----------------------------------------------------------------------
 * LED blink thread
 * --------------------------------------------------------------------- */

static void blinky_thread_entry(void *arg)
{
    (void)arg;

    /* Look up the GPIO device registered by the driver. */
    const vibe_device_t *gpio_dev = vibe_device_get("gpio0");
    if (gpio_dev == NULL) {
        vibe_printk("[blinky] ERROR: gpio0 device not found\n");
        return;
    }

    /* Configure LED pin as output, no pull. */
    vibe_err_t err = gpio_configure(gpio_dev, LED_PIN, GPIO_DIR_OUTPUT, GPIO_PULL_NONE);
    if (err != VIBE_OK) {
        vibe_printk("[blinky] gpio_configure failed: %ld\n", (long)err);
        return;
    }

    vibe_printk("[blinky] LED blink started on GPIO %u\n", LED_PIN);

    for (;;) {
        gpio_toggle(gpio_dev, LED_PIN);
        vibe_thread_sleep(BLINK_MS);
    }
}

VIBE_THREAD_DEFINE(blinky_thread, 512, blinky_thread_entry, NULL, 16);

/* -----------------------------------------------------------------------
 * main()
 * --------------------------------------------------------------------- */

int main(void)
{
    vibe_init();
    return 0;
}
