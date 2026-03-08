#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

/**
 * @file drivers/uart.h
 * @brief UART driver API for VibeRTOS.
 */

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_ODD  = 1,
    UART_PARITY_EVEN = 2,
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_2 = 1,
} uart_stop_bits_t;

typedef enum {
    UART_FLOW_NONE    = 0,
    UART_FLOW_RTS_CTS = 1,
} uart_flow_ctrl_t;

typedef struct {
    uint32_t         baud_rate;
    uint8_t          data_bits;    /**< 5, 6, 7, or 8 */
    uart_parity_t    parity;
    uart_stop_bits_t stop_bits;
    uart_flow_ctrl_t flow_ctrl;
} uart_config_t;

typedef struct uart_ops {
    vibe_device_api_t base;
    vibe_err_t (*configure)(const vibe_device_t *dev, const uart_config_t *cfg);
    vibe_err_t (*tx_byte)  (const vibe_device_t *dev, uint8_t c);
    vibe_err_t (*rx_byte)  (const vibe_device_t *dev, uint8_t *c);
    int        (*tx_buf)   (const vibe_device_t *dev, const uint8_t *buf, size_t len);
    int        (*rx_buf)   (const vibe_device_t *dev, uint8_t *buf, size_t len,
                             vibe_tick_t timeout);
    vibe_err_t (*irq_enable)(const vibe_device_t *dev, bool rx, bool tx);
} uart_ops_t;

static inline vibe_err_t uart_configure(const vibe_device_t *dev,
                                          const uart_config_t *cfg)
{
    return ((const uart_ops_t *)dev->api)->configure(dev, cfg);
}

static inline vibe_err_t uart_putc(const vibe_device_t *dev, uint8_t c)
{
    return ((const uart_ops_t *)dev->api)->tx_byte(dev, c);
}

static inline vibe_err_t uart_getc(const vibe_device_t *dev, uint8_t *c)
{
    return ((const uart_ops_t *)dev->api)->rx_byte(dev, c);
}

static inline int uart_write(const vibe_device_t *dev,
                               const uint8_t *buf, size_t len)
{
    return ((const uart_ops_t *)dev->api)->tx_buf(dev, buf, len);
}

static inline int uart_read(const vibe_device_t *dev, uint8_t *buf,
                              size_t len, vibe_tick_t timeout)
{
    return ((const uart_ops_t *)dev->api)->rx_buf(dev, buf, len, timeout);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_UART_H */
