/**
 * @file lib/ring_buffer/ring_buffer.c
 * @brief Lock-free single-producer / single-consumer ring buffer implementation.
 *
 * Uses power-of-2 buffer sizes and bitmasking for efficient index wrapping.
 * The head (read) index is owned by the consumer; the tail (write) index
 * is owned by the producer. No locking is needed for a single P/C pair
 * because each reads only its own index as read-write.
 */

#include "ring_buffer.h"
#include <string.h>

bool vibe_rb_init(vibe_rb_t *rb, void *buf, size_t size)
{
    if (rb == NULL || buf == NULL || size == 0U) { return false; }
    if (!IS_POWER_OF_TWO(size)) { return false; }

    rb->buf  = (uint8_t *)buf;
    rb->size = size;
    rb->head = 0U;
    rb->tail = 0U;
#if defined(CONFIG_RING_BUFFER_STATS)
    rb->peak_used  = 0U;
    rb->put_count  = 0U;
    rb->get_count  = 0U;
    rb->drop_count = 0U;
#endif
    return true;
}

/* Internal: mask index to buffer range */
static inline size_t _mask(const vibe_rb_t *rb, size_t idx)
{
    return idx & (rb->size - 1U);
}

size_t vibe_rb_used(const vibe_rb_t *rb)
{
    /* tail - head (both modulo 2*size to allow full-vs-empty distinction) */
    return rb->tail - rb->head;
}

size_t vibe_rb_free(const vibe_rb_t *rb)
{
    return rb->size - vibe_rb_used(rb);
}

bool vibe_rb_put(vibe_rb_t *rb, uint8_t byte)
{
    if (vibe_rb_is_full(rb)) {
#if defined(CONFIG_RING_BUFFER_STATS)
        rb->drop_count++;
#endif
        return false;
    }

    rb->buf[_mask(rb, rb->tail)] = byte;
    /* Memory barrier before incrementing tail so consumer sees data first */
    __atomic_signal_fence(__ATOMIC_RELEASE);
    rb->tail++;
#if defined(CONFIG_RING_BUFFER_STATS)
    rb->put_count++;
    size_t used = vibe_rb_used(rb);
    if (used > rb->peak_used) { rb->peak_used = used; }
#endif
    return true;
}

bool vibe_rb_get(vibe_rb_t *rb, uint8_t *byte)
{
    if (vibe_rb_is_empty(rb)) { return false; }

    *byte = rb->buf[_mask(rb, rb->head)];
    __atomic_signal_fence(__ATOMIC_ACQUIRE);
    rb->head++;
#if defined(CONFIG_RING_BUFFER_STATS)
    rb->get_count++;
#endif
    return true;
}

size_t vibe_rb_put_buf(vibe_rb_t *rb, const void *data, size_t len)
{
    const uint8_t *src   = (const uint8_t *)data;
    size_t         space = vibe_rb_free(rb);
    size_t         n     = len < space ? len : space;
    size_t         tail_idx;
    size_t         chunk;

    /* Write in at most two contiguous chunks (wrapping). */
    tail_idx = _mask(rb, rb->tail);
    chunk    = rb->size - tail_idx;

    if (chunk > n) { chunk = n; }
    memcpy(rb->buf + tail_idx, src, chunk);

    if (n > chunk) {
        memcpy(rb->buf, src + chunk, n - chunk);
    }

    __atomic_signal_fence(__ATOMIC_RELEASE);
    rb->tail += n;
#if defined(CONFIG_RING_BUFFER_STATS)
    rb->put_count  += n;
    rb->drop_count += len - n;
    size_t used = vibe_rb_used(rb);
    if (used > rb->peak_used) { rb->peak_used = used; }
#endif
    return n;
}

size_t vibe_rb_get_buf(vibe_rb_t *rb, void *buf, size_t len)
{
    uint8_t *dst     = (uint8_t *)buf;
    size_t   avail   = vibe_rb_used(rb);
    size_t   n       = len < avail ? len : avail;
    size_t   head_idx;
    size_t   chunk;

    head_idx = _mask(rb, rb->head);
    chunk    = rb->size - head_idx;

    if (chunk > n) { chunk = n; }
    memcpy(dst, rb->buf + head_idx, chunk);

    if (n > chunk) {
        memcpy(dst + chunk, rb->buf, n - chunk);
    }

    __atomic_signal_fence(__ATOMIC_ACQUIRE);
    rb->head += n;
#if defined(CONFIG_RING_BUFFER_STATS)
    rb->get_count += n;
#endif
    return n;
}

#if defined(CONFIG_RING_BUFFER_STATS)

void vibe_rb_get_stats(const vibe_rb_t *rb, vibe_rb_stats_t *s)
{
    if (rb == NULL || s == NULL) { return; }
    s->peak_used  = rb->peak_used;
    s->put_count  = rb->put_count;
    s->get_count  = rb->get_count;
    s->drop_count = rb->drop_count;
}

void vibe_rb_reset_stats(vibe_rb_t *rb)
{
    if (rb == NULL) { return; }
    rb->peak_used  = 0U;
    rb->put_count  = 0U;
    rb->get_count  = 0U;
    rb->drop_count = 0U;
}

#endif /* CONFIG_RING_BUFFER_STATS */
