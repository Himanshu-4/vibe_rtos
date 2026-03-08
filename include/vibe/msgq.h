#ifndef VIBE_MSGQ_H
#define VIBE_MSGQ_H

/**
 * @file msgq.h
 * @brief Fixed-size message queue (FIFO) API for VibeRTOS.
 *
 * Each message is a fixed-size block of bytes. The queue is backed by
 * a statically or dynamically allocated ring buffer.
 * Both senders and receivers can block with a timeout.
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Message queue struct
 * --------------------------------------------------------------------- */

typedef struct vibe_msgq {
    uint8_t     *buffer;     /**< Raw byte buffer (msg_size * max_msgs bytes). */
    size_t       msg_size;   /**< Size of each individual message in bytes. */
    uint32_t     max_msgs;   /**< Maximum number of messages in the queue. */
    uint32_t     head;       /**< Read index (messages are taken from here). */
    uint32_t     tail;       /**< Write index (messages are put here). */
    uint32_t     used;       /**< Number of messages currently queued. */
    vibe_list_t  read_wait;  /**< Threads waiting to read (queue empty). */
    vibe_list_t  write_wait; /**< Threads waiting to write (queue full). */
} vibe_msgq_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_MSGQ_DEFINE — statically define a message queue with its buffer.
 *
 * @param name      C identifier.
 * @param msg_sz    Size of each message in bytes.
 * @param max_msgs  Maximum number of queued messages.
 */
#define VIBE_MSGQ_DEFINE(name, msg_sz, max_msgs_count)              \
    static uint8_t _##name##_buf[(msg_sz) * (max_msgs_count)]       \
        __attribute__((aligned(4)));                                  \
    vibe_msgq_t name = {                                             \
        .buffer   = _##name##_buf,                                   \
        .msg_size = (msg_sz),                                        \
        .max_msgs = (max_msgs_count),                                \
        .head     = 0U,                                              \
        .tail     = 0U,                                              \
        .used     = 0U,                                              \
    }

/* -----------------------------------------------------------------------
 * Message queue API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a message queue.
 *
 * @param q         Queue to initialise.
 * @param buffer    Pre-allocated buffer of size (msg_size * max_msgs).
 * @param msg_size  Size of each message in bytes.
 * @param max_msgs  Maximum number of messages.
 * @return          VIBE_OK or VIBE_EINVAL.
 */
vibe_err_t vibe_msgq_init(vibe_msgq_t *q,
                           void        *buffer,
                           size_t       msg_size,
                           uint32_t     max_msgs);

/**
 * @brief Put a message into the queue, blocking if full.
 *
 * Copies msg_size bytes from data into the queue.
 *
 * @param q        Message queue.
 * @param data     Pointer to the message to copy in.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or ticks.
 * @return         VIBE_OK, VIBE_ETIMEOUT (full), VIBE_EAGAIN (no-wait full).
 */
vibe_err_t vibe_msgq_put(vibe_msgq_t *q, const void *data, vibe_tick_t timeout);

/**
 * @brief Get a message from the queue, blocking if empty.
 *
 * Copies msg_size bytes from the queue into buf.
 *
 * @param q        Message queue.
 * @param buf      Buffer to receive the message.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or ticks.
 * @return         VIBE_OK, VIBE_ETIMEOUT (empty), VIBE_EAGAIN (no-wait empty).
 */
vibe_err_t vibe_msgq_get(vibe_msgq_t *q, void *buf, vibe_tick_t timeout);

/**
 * @brief Peek at the head message without removing it.
 *
 * @param q    Message queue.
 * @param buf  Buffer to copy the head message into.
 * @return     VIBE_OK or VIBE_EAGAIN if empty.
 */
vibe_err_t vibe_msgq_peek(const vibe_msgq_t *q, void *buf);

/**
 * @brief Purge all messages from the queue.
 *
 * Wakes all blocked writers after clearing the queue.
 *
 * @param q  Message queue to purge.
 */
void vibe_msgq_purge(vibe_msgq_t *q);

/**
 * @brief Return the number of messages currently in the queue.
 *
 * @param q  Message queue.
 * @return   Number of messages available for reading.
 */
uint32_t vibe_msgq_count(const vibe_msgq_t *q);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_MSGQ_H */
