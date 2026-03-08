/**
 * @file drivers/uart/uart.c
 * @brief UART driver stub for VibeRTOS.
 *
 * USER: Replace register accesses with your SoC's UART peripheral.
 *
 * RP2040/RP2350 UART:
 *   Base address UART0: 0x40034000, UART1: 0x40038000
 *   Registers follow the ARM PL011 UART specification.
 *   Key registers: UARTDR, UARTFR, UARTIBRD, UARTFBRD, UARTLCRH, UARTCR
 */

#include "drivers/uart.h"
#include "vibe/device.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include <string.h>

#ifdef CONFIG_UART

/* -----------------------------------------------------------------------
 * PL011 UART register layout (RP2040/RP2350 compatible)
 * --------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t DR;       /* 0x000 — Data register */
    volatile uint32_t RSR_ECR;  /* 0x004 — Receive status / Error clear */
    uint32_t          _rsvd0[4];
    volatile uint32_t FR;       /* 0x018 — Flag register */
    uint32_t          _rsvd1;
    volatile uint32_t ILPR;     /* 0x020 — IrDA low-power counter */
    volatile uint32_t IBRD;     /* 0x024 — Integer baud rate divisor */
    volatile uint32_t FBRD;     /* 0x028 — Fractional baud rate divisor */
    volatile uint32_t LCRH;     /* 0x02C — Line control */
    volatile uint32_t CR;       /* 0x030 — Control */
    volatile uint32_t IFLS;     /* 0x034 — Interrupt FIFO level select */
    volatile uint32_t IMSC;     /* 0x038 — Interrupt mask set/clear */
    volatile uint32_t RIS;      /* 0x03C — Raw interrupt status */
    volatile uint32_t MIS;      /* 0x040 — Masked interrupt status */
    volatile uint32_t ICR;      /* 0x044 — Interrupt clear */
} _pl011_uart_t;

#define UART_FR_TXFF    (1U << 5)  /* TX FIFO full */
#define UART_FR_RXFE    (1U << 4)  /* RX FIFO empty */
#define UART_FR_BUSY    (1U << 3)  /* UART busy */

#define UART_LCRH_WLEN_8BIT  (0x3U << 5)
#define UART_LCRH_FEN        (1U << 4)  /* Enable FIFOs */

#define UART_CR_UARTEN  (1U << 0)
#define UART_CR_TXE     (1U << 8)
#define UART_CR_RXE     (1U << 9)

/* -----------------------------------------------------------------------
 * Driver config/data structs
 * --------------------------------------------------------------------- */

typedef struct {
    uintptr_t base_addr;   /**< UART peripheral base address */
    uint32_t  clock_hz;    /**< UART peripheral clock frequency */
} uart_config_hw_t;

typedef struct {
    uart_config_t current_cfg;
} uart_data_t;

/* -----------------------------------------------------------------------
 * Ops
 * --------------------------------------------------------------------- */

static vibe_err_t _uart_configure(const vibe_device_t *dev,
                                   const uart_config_t *cfg)
{
    const uart_config_hw_t *hw = (const uart_config_hw_t *)dev->config;
    _pl011_uart_t *uart = (_pl011_uart_t *)hw->base_addr;
    (void)uart;

    /*
     * TODO (USER): Configure the PL011 UART.
     *
     * 1. Disable UART:  uart->CR = 0;
     * 2. Wait for end of TX: while (uart->FR & UART_FR_BUSY);
     * 3. Set baud rate divisor:
     *      uint32_t div = (hw->clock_hz * 4) / cfg->baud_rate;
     *      uart->IBRD = div >> 6;
     *      uart->FBRD = div & 0x3F;
     * 4. Set data format: uart->LCRH = UART_LCRH_WLEN_8BIT | UART_LCRH_FEN;
     * 5. Enable:  uart->CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
     */
    vibe_printk("[uart] configure baud=%u\n", (unsigned)cfg->baud_rate);
    return VIBE_OK;
}

static vibe_err_t _uart_tx_byte(const vibe_device_t *dev, uint8_t c)
{
    const uart_config_hw_t *hw = (const uart_config_hw_t *)dev->config;
    _pl011_uart_t *uart = (_pl011_uart_t *)hw->base_addr;
    (void)uart;

    /*
     * TODO (USER): Polling TX.
     *   while (uart->FR & UART_FR_TXFF);   // wait until TX FIFO not full
     *   uart->DR = c;
     */
    return VIBE_OK;
}

static vibe_err_t _uart_rx_byte(const vibe_device_t *dev, uint8_t *c)
{
    const uart_config_hw_t *hw = (const uart_config_hw_t *)dev->config;
    _pl011_uart_t *uart = (_pl011_uart_t *)hw->base_addr;
    (void)uart;

    /*
     * TODO (USER): Polling RX.
     *   if (uart->FR & UART_FR_RXFE) return VIBE_EAGAIN;
     *   *c = (uint8_t)(uart->DR & 0xFF);
     */
    (void)c;
    return VIBE_EAGAIN;
}

static int _uart_tx_buf(const vibe_device_t *dev,
                         const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        _uart_tx_byte(dev, buf[i]);
    }
    return (int)len;
}

static int _uart_rx_buf(const vibe_device_t *dev, uint8_t *buf,
                         size_t len, vibe_tick_t timeout)
{
    size_t n = 0;
    (void)timeout;
    while (n < len) {
        if (_uart_rx_byte(dev, &buf[n]) == VIBE_OK) {
            n++;
        } else {
            break;
        }
    }
    return (int)n;
}

static vibe_err_t _uart_irq_enable(const vibe_device_t *dev, bool rx, bool tx)
{
    (void)dev; (void)rx; (void)tx;
    /* TODO: configure UARTIMSC and enable NVIC IRQ */
    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Init
 * --------------------------------------------------------------------- */

static vibe_err_t _uart_init(const vibe_device_t *dev)
{
    uart_config_t default_cfg = {
        .baud_rate = 115200U,
        .data_bits = 8,
        .parity    = UART_PARITY_NONE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_FLOW_NONE,
    };
    return _uart_configure(dev, &default_cfg);
}

/* -----------------------------------------------------------------------
 * Registration
 * --------------------------------------------------------------------- */

static const uart_config_hw_t _uart0_hw = {
    .base_addr = 0x40034000U,  /* RP2040 UART0 base */
    .clock_hz  = 125000000U,   /* TODO: set to actual UART peripheral clock */
};

static uart_data_t _uart0_data;

static const uart_ops_t _uart0_ops = {
    .base       = { .type = "uart" },
    .configure  = _uart_configure,
    .tx_byte    = _uart_tx_byte,
    .rx_byte    = _uart_rx_byte,
    .tx_buf     = _uart_tx_buf,
    .rx_buf     = _uart_rx_buf,
    .irq_enable = _uart_irq_enable,
};

VIBE_DEVICE_DEFINE(uart0, _uart_init, &_uart0_hw, &_uart0_data,
                   &_uart0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_UART */
