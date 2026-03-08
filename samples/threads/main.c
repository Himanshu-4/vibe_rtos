/**
 * @file samples/threads/main.c
 * @brief VibeRTOS two-thread producer/consumer example.
 *
 * Demonstrates:
 *  - Two threads communicating via a message queue
 *  - Producer creates messages at 200ms intervals
 *  - Consumer receives and prints them
 *  - VIBE_MSGQ_DEFINE for static queue allocation
 */

#include <vibe/kernel.h>

/* -----------------------------------------------------------------------
 * Message type
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t sequence;    /**< Message sequence number. */
    uint32_t timestamp_ms;/**< Time when produced. */
    uint32_t data;        /**< Payload. */
} sample_msg_t;

/* -----------------------------------------------------------------------
 * Shared message queue (4 messages deep)
 * --------------------------------------------------------------------- */

VIBE_MSGQ_DEFINE(sample_queue, sizeof(sample_msg_t), 4);

/* -----------------------------------------------------------------------
 * Producer thread — sends a message every 200ms
 * --------------------------------------------------------------------- */

static void producer_entry(void *arg)
{
    (void)arg;

    uint32_t seq = 0;

    vibe_printk("[producer] starting\n");

    for (;;) {
        sample_msg_t msg = {
            .sequence     = seq++,
            .timestamp_ms = vibe_uptime_ms(),
            .data         = seq * 0x1234U,
        };

        vibe_err_t err = vibe_msgq_put(&sample_queue, &msg, VIBE_MS_TO_TICKS(100));
        if (err != VIBE_OK) {
            vibe_printk("[producer] WARNING: queue full (err=%d)\n", err);
        }

        vibe_thread_sleep(200);
    }
}

/* -----------------------------------------------------------------------
 * Consumer thread — receives and prints messages
 * --------------------------------------------------------------------- */

static void consumer_entry(void *arg)
{
    (void)arg;

    vibe_printk("[consumer] starting\n");

    for (;;) {
        sample_msg_t msg;

        /* Block until a message is available */
        vibe_err_t err = vibe_msgq_get(&sample_queue, &msg, VIBE_WAIT_FOREVER);
        if (err == VIBE_OK) {
            vibe_printk("[consumer] seq=%u ts=%ums data=0x%x\n",
                        msg.sequence,
                        msg.timestamp_ms,
                        msg.data);
        } else {
            vibe_printk("[consumer] ERROR: msgq_get failed (%d)\n", err);
        }
    }
}

/* -----------------------------------------------------------------------
 * Thread definitions
 * --------------------------------------------------------------------- */

VIBE_THREAD_DEFINE(producer_thread, 512, producer_entry, NULL, 16);
VIBE_THREAD_DEFINE(consumer_thread, 512, consumer_entry, NULL, 15);

/* -----------------------------------------------------------------------
 * main()
 * --------------------------------------------------------------------- */

int main(void)
{
    vibe_printk("[main] VibeRTOS producer/consumer sample\n");
    vibe_init();
    return 0;
}
