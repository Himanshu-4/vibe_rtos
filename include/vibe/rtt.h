#ifndef VIBE_RTT_H
#define VIBE_RTT_H

/**
 * @file rtt.h
 * @brief Real-Time Transfer (RTT) — debugger-visible I/O channels.
 *
 * A SEGGER-RTT-compatible implementation: the control block layout and
 * its "SEGGER RTT" identifier match the de-facto standard, so standard
 * host tooling finds it without any target-side protocol:
 *
 *   - J-Link:   JLinkRTTViewer / JLinkRTTClient / RTT in Ozone
 *   - OpenOCD:  `rtt setup <ram_start> <ram_size> "SEGGER RTT"`,
 *               `rtt start`, `rtt server start <port> 0`
 *   - pyOCD:    `pyocd rtt`
 *
 * Data written with vibe_rtt_write() lands in a RAM ring buffer that the
 * debug probe drains in the background over SWD — no UART, no CPU-blocking
 * I/O. Channel 0 is the "Terminal" channel used by the CONFIG_LOG_BACKEND_RTT
 * logging backend and (optionally) the console.
 *
 * Writes are non-blocking: when the host is not draining the buffer and it
 * fills up, new data is dropped (up-buffer mode SKIP). This keeps timing
 * behaviour deterministic — logging never stalls the kernel.
 *
 * Enable with CONFIG_RTT=y.
 */

#include <stddef.h>
#include <stdint.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * SEGGER-compatible control block layout
 *
 * Field names and ordering MUST match SEGGER RTT so host tools can parse
 * the block. Do not reorder.
 * --------------------------------------------------------------------- */

/** Up buffer: target -> host (debug probe reads WrOff, updates RdOff). */
typedef struct {
    const char        *sName;         /**< Buffer name, e.g. "Terminal". */
    char              *pBuffer;       /**< Ring buffer storage. */
    unsigned int       SizeOfBuffer;  /**< Storage size in bytes. */
    unsigned int       WrOff;         /**< Write offset (target writes). */
    volatile unsigned int RdOff;      /**< Read offset (host writes). */
    unsigned int       Flags;         /**< Operating mode. */
} vibe_rtt_up_t;

/** Down buffer: host -> target (target reads RdOff, host updates WrOff). */
typedef struct {
    const char        *sName;
    char              *pBuffer;
    unsigned int       SizeOfBuffer;
    volatile unsigned int WrOff;      /**< Write offset (host writes). */
    unsigned int       RdOff;         /**< Read offset (target writes). */
    unsigned int       Flags;
} vibe_rtt_down_t;

#ifndef CONFIG_RTT_MAX_UP_BUFFERS
#define CONFIG_RTT_MAX_UP_BUFFERS    2
#endif
#ifndef CONFIG_RTT_MAX_DOWN_BUFFERS
#define CONFIG_RTT_MAX_DOWN_BUFFERS  2
#endif

/** The RTT control block — found by host tools via the "SEGGER RTT" ID. */
typedef struct {
    char             acID[16];        /**< "SEGGER RTT" (set at init). */
    int              MaxNumUpBuffers;
    int              MaxNumDownBuffers;
    vibe_rtt_up_t    aUp[CONFIG_RTT_MAX_UP_BUFFERS];
    vibe_rtt_down_t  aDown[CONFIG_RTT_MAX_DOWN_BUFFERS];
} vibe_rtt_cb_t;

/** Up-buffer operating modes (Flags field). */
#define VIBE_RTT_MODE_SKIP   0U  /**< Drop new data when full (default). */
#define VIBE_RTT_MODE_TRIM   1U  /**< Write as much as fits, drop the rest. */
#define VIBE_RTT_MODE_BLOCK  2U  /**< Busy-wait for the host (debug only). */

/* -----------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise the RTT control block and channel-0 buffers.
 *
 * Called automatically on first write/read; call explicitly (e.g. from
 * board init) to have the block ready before the first output.
 */
void vibe_rtt_init(void);

/**
 * @brief Write data to an up (target -> host) channel. Non-blocking.
 *
 * Safe from thread and ISR context.
 *
 * @param channel  Up-buffer index (0 = Terminal).
 * @param data     Bytes to write.
 * @param len      Number of bytes.
 * @return         Number of bytes actually stored (< len if full in
 *                 SKIP/TRIM mode), or 0 for an invalid channel.
 */
unsigned int vibe_rtt_write(unsigned int channel,
                            const void *data, unsigned int len);

/**
 * @brief Read data from a down (host -> target) channel. Non-blocking.
 *
 * @param channel  Down-buffer index (0 = Terminal).
 * @param buf      Destination buffer.
 * @param len      Maximum bytes to read.
 * @return         Number of bytes read (0 if none pending).
 */
unsigned int vibe_rtt_read(unsigned int channel, void *buf, unsigned int len);

/**
 * @brief Write a single character to up-channel 0.
 */
static inline void vibe_rtt_putc(char c)
{
    (void)vibe_rtt_write(0U, &c, 1U);
}

/**
 * @brief Bytes currently pending in an up buffer (not yet read by host).
 */
unsigned int vibe_rtt_up_used(unsigned int channel);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_RTT_H */
