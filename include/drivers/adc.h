#ifndef DRIVERS_ADC_H
#define DRIVERS_ADC_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  channel;
    uint8_t  resolution_bits;  /**< ADC resolution (e.g., 12). */
    uint32_t oversampling;     /**< Oversampling ratio (1 = none). */
} adc_channel_cfg_t;

typedef struct {
    const adc_channel_cfg_t *channels;
    uint8_t                  num_channels;
    uint16_t                *result_buf;  /**< Output buffer (one entry per channel). */
} adc_sequence_t;

typedef struct adc_ops {
    vibe_device_api_t base;
    vibe_err_t (*read)(const vibe_device_t *dev, const adc_sequence_t *seq);
} adc_ops_t;

static inline vibe_err_t adc_read(const vibe_device_t *dev,
                                    const adc_sequence_t *seq)
{
    return ((const adc_ops_t *)dev->api)->read(dev, seq);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_ADC_H */
