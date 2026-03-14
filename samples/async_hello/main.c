/**
 * @file samples/async_hello/main.c
 * @brief Introduction to VibeRTOS async/job-scheduler.
 *
 * This sample runs THREE concurrent coroutines inside a SINGLE RTOS task
 * using the cooperative job scheduler.  No blocking RTOS calls are made
 * inside the coroutines — yielding back to the scheduler replaces
 * vibe_thread_sleep().
 *
 * Jobs and what they demonstrate
 * ──────────────────────────────
 *  led_job   (prio=20)  Toggles the on-board LED every 500 ms.
 *                       Shows: ASYNC_AWAIT on a tick deadline.
 *
 *  counter_job (prio=15) Prints an incrementing counter every 1000 ms.
 *                       Shows: interleaved execution with led_job; prio=15
 *                       so it always runs after led_job within the same tick.
 *
 *  stats_job  (prio=5)  Prints scheduler diagnostics every 3000 ms, then
 *                       re-submits itself so it repeats indefinitely.
 *                       Shows: one-shot coroutine + self-resubmission.
 *                       Guarded by CONFIG_JOB_SCHEDULER_STATS.
 *
 * Timing model
 * ────────────
 *   Each coroutine stores its deadline in its context struct (survives yields).
 *   "Deadline" = tick at submission + VIBE_MS_TO_TICKS(period).
 *   ASYNC_AWAIT re-evaluates vibe_tick_get() >= deadline every scheduler tick.
 *
 * Priority model
 * ──────────────
 *   Higher prio ⟹ runs first in Phase-1 of vibe_sched_run().
 *   All three jobs are READY each tick; the scheduler runs them in order:
 *   led_job → counter_job → stats_job.
 *
 * Hardware: LED on GPIO 25 (Raspberry Pi Pico / Pico 2 on-board LED).
 */

#include <vibe/kernel.h>
#include <drivers/gpio.h>
#include "job_scheduler.h"     /* also pulls in async.h */

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define LED_PIN        25U
#define LED_PERIOD_MS  500U
#define LOG_PERIOD_MS  1000U
#define STATS_PERIOD_MS 3000U

/* -------------------------------------------------------------------------
 * Shared GPIO device — opened once by the RTOS task, shared via pointer.
 * ---------------------------------------------------------------------- */

static const vibe_device_t *g_gpio_dev;

/* -------------------------------------------------------------------------
 * Job contexts — all persistent state across yields lives here.
 * ---------------------------------------------------------------------- */

typedef struct {
    vibe_tick_t deadline;
    uint32_t    toggles;
} led_ctx_t;

typedef struct {
    vibe_tick_t deadline;
    uint32_t    count;
} counter_ctx_t;

/* One instance of each context (lives in BSS — zero-initialised). */
static led_ctx_t     g_led_ctx;
static counter_ctx_t g_counter_ctx;

#if defined(CONFIG_JOB_SCHEDULER_STATS)
typedef struct {
    vibe_tick_t       deadline;
    vibe_job_sched_t *sched;    /* back-pointer to re-submit self */
} stats_ctx_t;

static stats_ctx_t   g_stats_ctx;
#endif

/* -------------------------------------------------------------------------
 * Coroutine: led_job
 *
 * Toggles LED_PIN every LED_PERIOD_MS milliseconds.
 * Uses ASYNC_AWAIT to sleep until the deadline without blocking the RTOS.
 * ---------------------------------------------------------------------- */

static async_status_t led_job(void *arg, async_t *co)
{
    led_ctx_t *c = (led_ctx_t *)arg;

    ASYNC_BEGIN(co);

    for (;;) {
        gpio_toggle(g_gpio_dev, LED_PIN);
        c->toggles++;

        /* Arm deadline, then yield until it expires. */
        c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(LED_PERIOD_MS);
        ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);
    }

    ASYNC_END(co);
}

/* -------------------------------------------------------------------------
 * Coroutine: counter_job
 *
 * Prints an incrementing counter every LOG_PERIOD_MS milliseconds.
 * ---------------------------------------------------------------------- */

static async_status_t counter_job(void *arg, async_t *co)
{
    counter_ctx_t *c = (counter_ctx_t *)arg;

    ASYNC_BEGIN(co);

    for (;;) {
        c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(LOG_PERIOD_MS);
        ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);

        c->count++;
        vibe_printk("[async_hello] count=%lu  uptime=%lu ms\n",
                    (unsigned long)c->count,
                    (unsigned long)VIBE_TICKS_TO_MS(vibe_tick_get()));
    }

    ASYNC_END(co);
}

/* -------------------------------------------------------------------------
 * Coroutine: stats_job  (only when CONFIG_JOB_SCHEDULER_STATS=y)
 *
 * A one-shot coroutine: waits STATS_PERIOD_MS, prints diagnostics, then
 * re-submits itself so the pattern repeats.  Demonstrates that a coroutine
 * can schedule new work before it returns ASYNC_DONE.
 * ---------------------------------------------------------------------- */

#if defined(CONFIG_JOB_SCHEDULER_STATS)

static async_status_t stats_job(void *arg, async_t *co)
{
    stats_ctx_t *c = (stats_ctx_t *)arg;

    ASYNC_BEGIN(co);

    c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(STATS_PERIOD_MS);
    ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);

    {
        vibe_sched_stats_t s;
        vibe_sched_get_stats(c->sched, &s);
        vibe_printk("[async_hello] sched: submitted=%lu completed=%lu "
                    "active=%lu ticks=%lu\n",
                    (unsigned long)s.submitted,
                    (unsigned long)s.completed,
                    (unsigned long)s.active,
                    (unsigned long)s.ticks);
    }

    /* Re-submit so the stats job fires again after another period. */
    vibe_sched_submit(c->sched, stats_job, c, 5U);

    ASYNC_END(co);
}

#endif /* CONFIG_JOB_SCHEDULER_STATS */

/* -------------------------------------------------------------------------
 * RTOS task — owns the scheduler and drives all three coroutines.
 * ---------------------------------------------------------------------- */

static vibe_job_sched_t g_sched;

static void async_hello_task(void *arg)
{
    (void)arg;

    /* Open GPIO device and configure LED pin as output. */
    g_gpio_dev = vibe_device_get("gpio0");
    if (g_gpio_dev == NULL) {
        vibe_printk("[async_hello] ERROR: gpio0 not found\n");
        return;
    }
    gpio_configure(g_gpio_dev, LED_PIN, GPIO_DIR_OUTPUT, GPIO_PULL_NONE);

    /* Initialise the scheduler. */
    vibe_sched_init(&g_sched);

    /* Submit all jobs.  Higher prio = runs first in each scheduler tick. */
    vibe_sched_submit(&g_sched, led_job,     &g_led_ctx,     20U);
    vibe_sched_submit(&g_sched, counter_job, &g_counter_ctx, 15U);

#if defined(CONFIG_JOB_SCHEDULER_STATS)
    g_stats_ctx.sched = &g_sched;
    vibe_sched_submit(&g_sched, stats_job, &g_stats_ctx, 5U);
#endif

    vibe_printk("[async_hello] scheduler started — %d jobs\n",
                vibe_sched_active_count(&g_sched));

    /*
     * Main scheduler loop.
     *
     * vibe_sched_run() executes one full tick:
     *   Phase 1: all READY  jobs run once (led → counter → stats by priority).
     *   Phase 2: all BLOCKED jobs are re-probed (ASYNC_AWAIT condition check).
     *
     * When nothing is READY (all jobs are BLOCKED waiting for their deadlines),
     * vibe_sched_is_idle() returns true and we yield the RTOS task so other
     * RTOS threads (e.g. idle) can run.
     */
    for (;;) {
        vibe_sched_run(&g_sched);
        if (vibe_sched_is_idle(&g_sched)) {
            vibe_thread_yield();
        }
    }
}

/* Stack: 1 KB — comfortably fits the scheduler struct + call frames. */
VIBE_THREAD_DEFINE(async_hello_thread, 1024, async_hello_task, NULL, 16);

/* -------------------------------------------------------------------------
 * main()
 * ---------------------------------------------------------------------- */

int main(void)
{
    vibe_init();
    return 0;
}
