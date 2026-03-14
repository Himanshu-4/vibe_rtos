/**
 * @file samples/async_pipeline/main.c
 * @brief Async data pipeline: sampler → processor → printer.
 *
 * Architecture
 * ────────────
 *
 *   sampler_job           processor_job          printer_job
 *   ───────────           ─────────────          ───────────
 *   Generates a new       Waits for a raw         Waits for a processed
 *   synthetic ADC         sample in g_raw_buf,    value in g_proc_buf,
 *   sample every 100 ms   computes a 4-sample     then prints the result
 *   and puts it into      moving average and       and a drop-rate stat.
 *   g_raw_buf.            puts it into g_proc_buf.
 *
 *   ┌──────────┐  uint32_t  ┌───────────┐  uint32_t  ┌─────────┐
 *   │ sampler  │ ─────────► │ processor │ ─────────► │ printer │
 *   └──────────┘  g_raw_buf └───────────┘  g_proc_buf└─────────┘
 *
 * Key patterns demonstrated
 * ─────────────────────────
 *  1. ASYNC_AWAIT(co, deadline)   — rate-limit the sampler without blocking.
 *  2. ASYNC_AWAIT(co, bytes ≥ N)  — suspend until data is ready to read.
 *  3. Ring buffer as inter-job FIFO — no mutexes, no RTOS queues.
 *  4. CONFIG_RING_BUFFER_STATS    — optional per-buffer drop/throughput stats.
 *  5. All three stages in ONE RTOS task, zero extra stack per coroutine.
 *
 * Note on local variables across yields
 * ──────────────────────────────────────
 * Duff's-device coroutines lose their stack frame on every yield/await.
 * Variables that must survive a yield live in the ctx struct.
 * Variables used only BETWEEN two yield points can be locals (they are
 * assigned fresh after the yield returns).
 */

#include <vibe/kernel.h>
#include "job_scheduler.h"
#include "ring_buffer.h"

/* -------------------------------------------------------------------------
 * Pipeline parameters
 * ---------------------------------------------------------------------- */

#define SAMPLE_PERIOD_MS   100U   /* sampler fires every 100 ms  */
#define AVG_WINDOW         4U     /* moving average window length */
#define PRINT_EVERY        10U    /* printer reports every N results */

/* Ring-buffer capacity: must be a power of 2 and hold multiple uint32_t. */
#define RAW_BUF_BYTES      64U    /* holds up to 16 raw uint32_t samples  */
#define PROC_BUF_BYTES     64U    /* holds up to 16 averaged uint32_t values */

/* -------------------------------------------------------------------------
 * Inter-job ring buffers
 *
 * VIBE_RING_BUF_DEFINE expands to a static backing array + vibe_rb_t.
 * ASYNC_AWAIT conditions below read vibe_rb_used() on these globals.
 * ---------------------------------------------------------------------- */

VIBE_RING_BUF_DEFINE(g_raw_buf,  RAW_BUF_BYTES);
VIBE_RING_BUF_DEFINE(g_proc_buf, PROC_BUF_BYTES);

/* -------------------------------------------------------------------------
 * Job contexts (all state that must survive a yield)
 * ---------------------------------------------------------------------- */

typedef struct {
    vibe_tick_t deadline;   /* next sample time */
    uint32_t    seq;        /* sequence counter  */
} sampler_ctx_t;

typedef struct {
    uint32_t history[AVG_WINDOW]; /* circular window for moving average */
    uint8_t  hlen;                /* samples accumulated so far         */
    uint32_t produced;            /* total averaged values produced     */
} proc_ctx_t;

typedef struct {
    uint32_t received;  /* total values printed */
} printer_ctx_t;

static sampler_ctx_t  g_sampler_ctx;
static proc_ctx_t     g_proc_ctx;
static printer_ctx_t  g_printer_ctx;

/* -------------------------------------------------------------------------
 * Coroutine: sampler_job
 *
 * Generates a synthetic ADC reading every SAMPLE_PERIOD_MS milliseconds
 * and pushes it into g_raw_buf.  Uses ASYNC_AWAIT on a tick deadline for
 * rate-limiting — the coroutine is BLOCKED between samples.
 * ---------------------------------------------------------------------- */

static async_status_t sampler_job(void *arg, async_t *co)
{
    sampler_ctx_t *c   = (sampler_ctx_t *)arg;
    uint32_t       sample;  /* local — used entirely between yield points */

    ASYNC_BEGIN(co);

    for (;;) {
        /* Synthesise an ADC-like value: sawtooth 0–255 with small noise. */
        sample = (c->seq * 7U) & 0xFFU;

        if (vibe_rb_put_buf(&g_raw_buf, &sample, sizeof(sample))
                < sizeof(sample)) {
            /* Buffer full — the processor is lagging behind. */
            vibe_printk("[pipeline] sampler: raw buf full, sample dropped\n");
        }
        c->seq++;

        /* Wait until the next sample period. */
        c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(SAMPLE_PERIOD_MS);
        ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);
    }

    ASYNC_END(co);
}

/* -------------------------------------------------------------------------
 * Coroutine: processor_job
 *
 * Waits for a raw sample in g_raw_buf, computes a moving average over a
 * window of AVG_WINDOW samples, and pushes the result into g_proc_buf.
 *
 * ASYNC_AWAIT on buffer occupancy (data-driven wakeup via polling).
 * ---------------------------------------------------------------------- */

static async_status_t processor_job(void *arg, async_t *co)
{
    proc_ctx_t *c = (proc_ctx_t *)arg;
    uint32_t raw, avg, sum, i; /* locals — used only between yield points */

    ASYNC_BEGIN(co);

    for (;;) {
        /* Suspend until at least one full uint32_t is in the raw buffer. */
        ASYNC_AWAIT(co, vibe_rb_used(&g_raw_buf) >= sizeof(uint32_t));

        vibe_rb_get_buf(&g_raw_buf, &raw, sizeof(raw));

        /* Accumulate into circular window. */
        c->history[c->hlen % AVG_WINDOW] = raw;
        c->hlen++;

        /* Compute average over the filled portion of the window. */
        sum = 0;
        for (i = 0; i < AVG_WINDOW && i < (uint32_t)c->hlen; i++) {
            sum += c->history[i];
        }
        avg = sum / (i > 0U ? i : 1U);

        if (vibe_rb_put_buf(&g_proc_buf, &avg, sizeof(avg))
                < sizeof(avg)) {
            vibe_printk("[pipeline] processor: proc buf full, result dropped\n");
        }
        c->produced++;
    }

    ASYNC_END(co);
}

/* -------------------------------------------------------------------------
 * Coroutine: printer_job
 *
 * Waits for a processed value in g_proc_buf and prints it.
 * Every PRINT_EVERY values it also emits optional ring-buffer stats.
 * ---------------------------------------------------------------------- */

static async_status_t printer_job(void *arg, async_t *co)
{
    printer_ctx_t *c = (printer_ctx_t *)arg;
    uint32_t       result; /* local — used between yield points only */

    ASYNC_BEGIN(co);

    for (;;) {
        /* Suspend until a processed uint32_t is ready. */
        ASYNC_AWAIT(co, vibe_rb_used(&g_proc_buf) >= sizeof(uint32_t));

        vibe_rb_get_buf(&g_proc_buf, &result, sizeof(result));
        c->received++;

        vibe_printk("[pipeline] avg=%lu  (n=%lu)\n",
                    (unsigned long)result,
                    (unsigned long)c->received);

#if defined(CONFIG_RING_BUFFER_STATS)
        /* Every PRINT_EVERY samples, print ring-buffer diagnostics. */
        if ((c->received % PRINT_EVERY) == 0U) {
            vibe_rb_stats_t rs;
            vibe_rb_get_stats(&g_raw_buf, &rs);
            vibe_printk("[pipeline] raw buf stats: put=%lu get=%lu "
                        "drop=%lu peak=%lu\n",
                        (unsigned long)rs.put_count,
                        (unsigned long)rs.get_count,
                        (unsigned long)rs.drop_count,
                        (unsigned long)rs.peak_used);
        }
#endif
    }

    ASYNC_END(co);
}

/* -------------------------------------------------------------------------
 * RTOS task — owns the scheduler and three pipeline stages.
 * ---------------------------------------------------------------------- */

static vibe_job_sched_t g_sched;

static void pipeline_task(void *arg)
{
    (void)arg;

    vibe_sched_init(&g_sched);

    /*
     * Priority assignment:
     *   sampler   (20) — runs first so data is always pushed before processing.
     *   processor (15) — transforms data before the printer reads.
     *   printer   (10) — reads last, ensuring the full pipeline ran this tick.
     */
    vibe_sched_submit(&g_sched, sampler_job,   &g_sampler_ctx,  20U);
    vibe_sched_submit(&g_sched, processor_job, &g_proc_ctx,     15U);
    vibe_sched_submit(&g_sched, printer_job,   &g_printer_ctx,  10U);

    vibe_printk("[pipeline] started — sampler=%d ms window=%d\n",
                SAMPLE_PERIOD_MS, AVG_WINDOW);

    for (;;) {
        vibe_sched_run(&g_sched);
        if (vibe_sched_is_idle(&g_sched)) {
            vibe_thread_yield();
        }
    }
}

VIBE_THREAD_DEFINE(pipeline_thread, 1024, pipeline_task, NULL, 16);

/* -------------------------------------------------------------------------
 * main()
 * ---------------------------------------------------------------------- */

int main(void)
{
    vibe_init();
    return 0;
}
