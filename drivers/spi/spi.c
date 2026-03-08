/**
 * @file drivers/spi/spi.c
 * @brief SPI driver stub for VibeRTOS.
 * USER: implement with your SoC's SPI registers (RP2040: SPI0/SPI1 PL022).
 */

#include "drivers/spi.h"
#include "vibe/device.h"
#include "vibe/types.h"

#ifdef CONFIG_SPI

static vibe_err_t _spi_transceive(const vibe_device_t *dev,
                                    const spi_config_t  *cfg,
                                    const void          *tx_buf,
                                    void                *rx_buf,
                                    size_t               len)
{
    (void)dev; (void)cfg; (void)tx_buf; (void)rx_buf; (void)len;
    /* TODO: implement PL022 SPI transfer */
    return VIBE_ENOSYS;
}

static vibe_err_t _spi_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const spi_ops_t _spi0_ops = {
    .base       = { .type = "spi" },
    .transceive = _spi_transceive,
};

VIBE_DEVICE_DEFINE(spi0, _spi_init, NULL, NULL,
                   &_spi0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_SPI */
