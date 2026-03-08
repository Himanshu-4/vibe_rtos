#ifndef DRIVERS_SPI_H
#define DRIVERS_SPI_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t  frequency_hz;
    uint8_t   mode;        /**< SPI mode 0-3 (CPOL/CPHA). */
    uint8_t   word_size;   /**< Bits per word (8, 16). */
    int32_t   cs_pin;      /**< Chip-select pin (-1 = manual). */
} spi_config_t;

typedef struct spi_ops {
    vibe_device_api_t base;
    vibe_err_t (*transceive)(const vibe_device_t *dev,
                              const spi_config_t  *cfg,
                              const void          *tx_buf,
                              void                *rx_buf,
                              size_t               len);
} spi_ops_t;

static inline vibe_err_t spi_transceive(const vibe_device_t *dev,
                                          const spi_config_t  *cfg,
                                          const void          *tx_buf,
                                          void                *rx_buf,
                                          size_t               len)
{
    return ((const spi_ops_t *)dev->api)->transceive(dev, cfg, tx_buf, rx_buf, len);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_SPI_H */
