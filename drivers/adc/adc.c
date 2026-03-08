/**
 * @file drivers/adc/adc.c
 * @brief ADC driver stub for VibeRTOS.
 * USER: Implement using RP2040 ADC peripheral (base 0x4004C000).
 *
 * RP2040 ADC registers:
 *   CS      0x00 — control and status
 *   RESULT  0x04 — conversion result
 *   FCS     0x08 — FIFO control
 *   FIFO    0x0C — FIFO data
 *   DIV     0x10 — clock divisor
 *   INTR    0x14 — raw interrupt
 *   INTE    0x18 — interrupt enable
 *   INTF    0x1C — interrupt force
 *   INTS    0x20 — interrupt status
 */

#include "drivers/adc.h"
#include "vibe/device.h"

#ifdef CONFIG_ADC

static vibe_err_t _adc_read(const vibe_device_t *dev,
                              const adc_sequence_t *seq)
{
    (void)dev; (void)seq;
    /*
     * TODO (USER): for each channel in seq:
     *   1. Select channel: ADC->CS = (ADC->CS & ~(0xF << 12)) | (ch << 12);
     *   2. Start conversion: ADC->CS |= ADC_CS_START_ONCE;
     *   3. Wait ready: while (!(ADC->CS & ADC_CS_READY));
     *   4. Read result: seq->result_buf[i] = ADC->RESULT & 0xFFF;
     */
    return VIBE_ENOSYS;
}

static vibe_err_t _adc_init(const vibe_device_t *dev)
{
    (void)dev;
    /* TODO: Enable ADC peripheral clock, power up ADC */
    return VIBE_OK;
}

static const adc_ops_t _adc0_ops = {
    .base = { .type = "adc" },
    .read = _adc_read,
};

VIBE_DEVICE_DEFINE(adc0, _adc_init, NULL, NULL,
                   &_adc0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_ADC */
