#ifndef VIBE_PIPE_H
#define VIBE_PIPE_H

/**
 * @file pipe.h
 * @brief Byte-stream pipe API for VibeRTOS.
 *
 * A pipe is a byte-stream buffer for inter-thread communication.
 * Unlike a message queue (which transfers fixed-size messages),
 * a pipe lets threads write and read arbitrary byte sequences.
 * Both partial reads/writes and blocking with timeout are supported.
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Pipe struct
 * --------------------------------------------------------------------- */

typedef struct vibe_pipe {
    uint8_t     *buf;         /**< Ring buffer storage. */
    size_t       size;        /**< Total buffer capacity in bytes. */
    size_t       head;        /**< Read pointer (next byte to consume). */
    size_t       tail;        /**< Write pointer (next byte to produce). */
    size_t       used;        /**< Bytes currently in the buffer. */
    vibe_list_t  read_wait;   /**< Threads blocked waiting to read. */
    vibe_list_t  write_wait;  /**< Threads blocked waiting to write. */
} vibe_pipe_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_PIPE_DEFINE — statically define a pipe with its backing buffer.
 *
 * @param name     C identifier.
 * @param buf_sz   Buffer size in bytes (best as power of 2).
 */
#define VIBE_PIPE_DEFINE(name, buf_sz)                  \
    static uint8_t _##name##_buf[buf_sz]                \
        __attribute__((aligned(4)));                     \
    vibe_pipe_t name = {                                 \
        .buf  = _##name##_buf,                           \
        .size = (buf_sz),                                \
        .head = 0U,                                      \
        .tail = 0U,                                      \
        .used = 0U,                                      \
    }

/* -----------------------------------------------------------------------
 * Pipe API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a pipe.
 *
 * @param pipe    Pipe to initialise.
 * @param buf     Pre-allocated backing buffer.
 * @param size    Buffer size in bytes.
 * @return        VIBE_OK or VIBE_EINVAL.
 */
vibe_err_t vibe_pipe_init(vibe_pipe_t *pipe, void *buf, size_t size);

/**
 * @brief Write bytes into the pipe, blocking if the buffer is full.
 *
 * Will block until at least len bytes have been written, or until timeout.
 * Returns the number of bytes actually written (may be < len on timeout).
 *
 * @param pipe     Pipe to write to.
 * @param data     Source data buffer.
 * @param len      Number of bytes to write.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or ticks.
 * @return         Number of bytes written, or negative error code.
 */
int vibe_pipe_write(vibe_pipe_t *pipe,
                    const void  *data,
                    size_t       len,
                    vibe_tick_t  timeout);

/**
 * @brief Read bytes from the pipe, blocking if the buffer is empty.
 *
 * Returns the number of bytes actually read (may be < len on timeout).
 *
 * @param pipe     Pipe to read from.
 * @param buf      Destination buffer.
 * @param len      Maximum number of bytes to read.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or ticks.
 * @return         Number of bytes read, or negative error code.
 */
int vibe_pipe_read(vibe_pipe_t *pipe,
                   void        *buf,
                   size_t       len,
                   vibe_tick_t  timeout);

/**
 * @brief Return the number of bytes available to read.
 *
 * @param pipe  Pipe to query.
 * @return      Bytes available.
 */
size_t vibe_pipe_available_read(const vibe_pipe_t *pipe);

/**
 * @brief Return the number of bytes of free space available for writing.
 *
 * @param pipe  Pipe to query.
 * @return      Free bytes.
 */
size_t vibe_pipe_available_write(const vibe_pipe_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_PIPE_H */
