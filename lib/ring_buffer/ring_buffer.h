#ifndef VIBE_RING_BUFFER_H
#define VIBE_RING_BUFFER_H

/**
 * @file lib/ring_buffer/ring_buffer.h
 * @brief Lock-free single-producer / single-consumer ring buffer.
 *
 * Optimised for power-of-2 sizes using bitmask wrapping.
 * Thread-safe for exactly one producer and one consumer without locking.
 * For multiple producers/consumers, add an external lock.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vibe/sys/util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Ring buffer struct
 * --------------------------------------------------------------------- */

typedef struct vibe_rb {
    uint8_t  *buf;   /**< Backing byte array. Must be size bytes large. */
    size_t    size;  /**< Buffer capacity — MUST be a power of 2. */
    size_t    head;  /**< Read index (consumer). */
    size_t    tail;  /**< Write index (producer). */
#if defined(CONFIG_RING_BUFFER_STATS)
    size_t    peak_used;    /**< Peak occupancy ever seen (bytes). */
    size_t    put_count;    /**< Total bytes successfully written. */
    size_t    get_count;    /**< Total bytes successfully read. */
    size_t    drop_count;   /**< Bytes dropped because the buffer was full. */
#endif
} vibe_rb_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_RING_BUF_DEFINE — statically define a ring buffer.
 *
 * @param name  C identifier.
 * @param sz    Buffer size in bytes — must be a power of 2.
 */
#if defined(CONFIG_RING_BUFFER_STATS)
#define _VIBE_RB_STATS_INIT \
    .peak_used  = 0U,       \
    .put_count  = 0U,       \
    .get_count  = 0U,       \
    .drop_count = 0U,
#else
#define _VIBE_RB_STATS_INIT
#endif

#define VIBE_RING_BUF_DEFINE(name, sz)                              \
    STATIC_ASSERT(IS_POWER_OF_TWO(sz),                              \
                  "Ring buffer size must be power of 2");           \
    static uint8_t _##name##_buf[sz];                               \
    vibe_rb_t name = {                                               \
        .buf  = _##name##_buf,                                       \
        .size = (sz),                                                \
        .head = 0U,                                                  \
        .tail = 0U,                                                  \
        _VIBE_RB_STATS_INIT                                          \
    }

/* -----------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a ring buffer.
 *
 * @param rb    Ring buffer to initialise.
 * @param buf   Backing byte array (must be size bytes, power-of-2).
 * @param size  Buffer size in bytes (must be power of 2).
 * @return      true on success, false if size is not a power of 2.
 */
bool vibe_rb_init(vibe_rb_t *rb, void *buf, size_t size);

/**
 * @brief Write one byte into the ring buffer.
 *
 * @param rb   Ring buffer.
 * @param byte Byte to write.
 * @return     true if written, false if full.
 */
bool vibe_rb_put(vibe_rb_t *rb, uint8_t byte);

/**
 * @brief Read one byte from the ring buffer.
 *
 * @param rb   Ring buffer.
 * @param byte Output byte.
 * @return     true if read, false if empty.
 */
bool vibe_rb_get(vibe_rb_t *rb, uint8_t *byte);

/**
 * @brief Write up to len bytes into the ring buffer.
 *
 * @param rb   Ring buffer.
 * @param data Source data.
 * @param len  Number of bytes to write.
 * @return     Number of bytes actually written.
 */
size_t vibe_rb_put_buf(vibe_rb_t *rb, const void *data, size_t len);

/**
 * @brief Read up to len bytes from the ring buffer.
 *
 * @param rb   Ring buffer.
 * @param buf  Destination buffer.
 * @param len  Maximum bytes to read.
 * @return     Number of bytes actually read.
 */
size_t vibe_rb_get_buf(vibe_rb_t *rb, void *buf, size_t len);

/**
 * @brief Return number of bytes available to read.
 */
size_t vibe_rb_used(const vibe_rb_t *rb);

/**
 * @brief Return number of free bytes available for writing.
 */
size_t vibe_rb_free(const vibe_rb_t *rb);

/** @return true if the buffer has no data. */
static inline bool vibe_rb_is_empty(const vibe_rb_t *rb)
{
    return rb->head == rb->tail;
}

/** @return true if the buffer is completely full. */
static inline bool vibe_rb_is_full(const vibe_rb_t *rb)
{
    return vibe_rb_used(rb) == rb->size;
}

#if defined(CONFIG_RING_BUFFER_STATS)

/**
 * Snapshot of ring buffer statistics.
 * Populated by vibe_rb_get_stats().
 */
typedef struct {
    size_t peak_used;    /**< Peak occupancy ever observed (bytes). */
    size_t put_count;    /**< Total bytes successfully written. */
    size_t get_count;    /**< Total bytes successfully read. */
    size_t drop_count;   /**< Bytes dropped because the buffer was full. */
} vibe_rb_stats_t;

/**
 * @brief Copy the current statistics out of a ring buffer.
 *
 * @param rb  Ring buffer to query.
 * @param s   Output stats struct (must not be NULL).
 */
void vibe_rb_get_stats(const vibe_rb_t *rb, vibe_rb_stats_t *s);

/**
 * @brief Reset all statistics counters to zero.
 *
 * @param rb  Ring buffer to reset.
 */
void vibe_rb_reset_stats(vibe_rb_t *rb);

#endif /* CONFIG_RING_BUFFER_STATS */

#ifdef __cplusplus
}
#endif

#endif /* VIBE_RING_BUFFER_H */
