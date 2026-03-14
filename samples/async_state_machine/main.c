/**
 * @file samples/async_state_machine/main.c
 * @brief Coroutine composition with ASYNC_AWAIT_CORO.
 *
 * System lifecycle state machine
 * ───────────────────────────────
 *
 *   ┌─────────────────────────────────────────────────────────┐
 *   │                   fsm_job (top level)                    │
 *   │                                                          │
 *   │  STATE_BOOT ──► STATE_OPERATIONAL ──► STATE_FAULT        │
 *   │      │                                      │            │
 *   │      └──────────────────────────────────────┘            │
 *   │               (fault resets to BOOT)                     │
 *   └─────────────────────────────────────────────────────────┘
 *
 * STATE_BOOT
 *   Runs three initialisation sub-coroutines IN SEQUENCE via
 *   ASYNC_AWAIT_CORO.  Each sub-coroutine simulates hardware init
 *   with a time delay and a progress log.
 *
 *   init_hw     → 600 ms (peripheral clock, GPIO)
 *   init_periph → 400 ms (UART, SPI buses)
 *   init_app    → 200 ms (application state, default config)
 *
 * STATE_OPERATIONAL
 *   Runs two sub-coroutines CONCURRENTLY via a nested scheduler:
 *
 *   heartbeat_job  — prints a keep-alive every 1000 ms
 *   sensor_job     — polls a synthetic sensor every 250 ms;
 *                    triggers a fault if the value exceeds a threshold
 *
 * STATE_FAULT
 *   Logs the fault, waits 2000 ms, then resets the system back to BOOT.
 *
 * Key patterns shown
 * ──────────────────
 *  ASYNC_AWAIT_CORO(co, &child_co, fn, ctx)
 *      Parent suspends until the child coroutine returns ASYNC_DONE.
 *      The parent coroutine advances by one step each scheduler tick.
 *
 *  Nested scheduler (g_op_sched inside STATE_OPERATIONAL)
 *      One scheduler can host a sub-scheduler for CONCURRENT sub-tasks.
 *      The inner scheduler is driven by the outer job on each tick.
 *
 *  async_init(&co) before ASYNC_AWAIT_CORO
 *      Resets a child's resume-point so it restarts from the beginning
 *      if the parent re-enters the AWAIT_CORO block.
 *
 *  ASYNC_RETURN(co)
 *      Exits the sensor_job early when a fault is detected.
 */

#include <vibe/kernel.h>
#include "job_scheduler.h"

/* -------------------------------------------------------------------------
 * State machine states
 * ---------------------------------------------------------------------- */

typedef enum {
    STATE_BOOT        = 0,
    STATE_OPERATIONAL = 1,
    STATE_FAULT       = 2,
} fsm_state_t;

/* Shared flag: sensor_job sets this to 1 to signal a fault to fsm_job. */
static volatile int g_fault_detected;

/* =========================================================================
 * BOOT sub-coroutines
 * ====================================================================== */

typedef struct { vibe_tick_t deadline; } init_ctx_t;

/* Simulated hardware init — 600 ms delay. */
static async_status_t init_hw(void *arg, async_t *co)
{
    init_ctx_t *c = (init_ctx_t *)arg;
    ASYNC_BEGIN(co);

    vibe_printk("[boot] init_hw: starting peripheral clocks...\n");
    c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(600);
    ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);
    vibe_printk("[boot] init_hw: done\n");

    ASYNC_END(co);
}

/* Simulated peripheral init — 400 ms delay. */
static async_status_t init_periph(void *arg, async_t *co)
{
    init_ctx_t *c = (init_ctx_t *)arg;
    ASYNC_BEGIN(co);

    vibe_printk("[boot] init_periph: configuring buses...\n");
    c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(400);
    ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);
    vibe_printk("[boot] init_periph: done\n");

    ASYNC_END(co);
}

/* Simulated application init — 200 ms delay. */
static async_status_t init_app(void *arg, async_t *co)
{
    init_ctx_t *c = (init_ctx_t *)arg;
    ASYNC_BEGIN(co);

    vibe_printk("[boot] init_app: loading default config...\n");
    c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(200);
    ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);
    vibe_printk("[boot] init_app: done\n");

    ASYNC_END(co);
}

/* =========================================================================
 * OPERATIONAL sub-coroutines
 * ====================================================================== */

typedef struct {
    vibe_tick_t deadline;
    uint32_t    count;
} hb_ctx_t;

typedef struct {
    vibe_tick_t deadline;
    uint32_t    reading;
} sensor_ctx_t;

/* Heartbeat: prints a keep-alive every 1000 ms. */
static async_status_t heartbeat_job(void *arg, async_t *co)
{
    hb_ctx_t *c = (hb_ctx_t *)arg;
    ASYNC_BEGIN(co);

    for (;;) {
        c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(1000);
        ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);

        c->count++;
        vibe_printk("[op] heartbeat #%lu\n", (unsigned long)c->count);
    }

    ASYNC_END(co);
}

#define FAULT_THRESHOLD  200U

/* Sensor poll: samples a synthetic value every 250 ms.
 * If the value exceeds FAULT_THRESHOLD, sets g_fault_detected and exits. */
static async_status_t sensor_job(void *arg, async_t *co)
{
    sensor_ctx_t *c = (sensor_ctx_t *)arg;
    ASYNC_BEGIN(co);

    for (;;) {
        c->deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(250);
        ASYNC_AWAIT(co, vibe_tick_get() >= c->deadline);

        /* Synthetic reading: slowly ramps up until it triggers the fault. */
        c->reading = (c->reading + 3U) & 0xFFU;

        vibe_printk("[op] sensor reading=%lu\n", (unsigned long)c->reading);

        if (c->reading >= FAULT_THRESHOLD) {
            vibe_printk("[op] FAULT: sensor exceeded threshold (%lu >= %u)\n",
                        (unsigned long)c->reading, FAULT_THRESHOLD);
            g_fault_detected = 1;
            ASYNC_RETURN(co); /* Early exit — returns ASYNC_DONE */
        }
    }

    ASYNC_END(co);
}

/* =========================================================================
 * Top-level FSM coroutine
 * ====================================================================== */

/*
 * All sub-coroutine states live in fsm_ctx_t so they survive FSM yields.
 * The nested scheduler (g_op_sched) is also here so it persists across
 * OPERATIONAL ticks.
 */
typedef struct {
    /* FSM */
    fsm_state_t state;

    /* BOOT child states */
    init_ctx_t  init_c;
    async_t     hw_co;
    async_t     periph_co;
    async_t     app_co;

    /* OPERATIONAL child states */
    hb_ctx_t     hb_c;
    sensor_ctx_t sensor_c;
    vibe_job_sched_t op_sched; /* nested scheduler for concurrent sub-jobs */

    /* FAULT deadline */
    vibe_tick_t  fault_deadline;
    uint32_t     fault_count;
} fsm_ctx_t;

static fsm_ctx_t g_fsm;

static async_status_t fsm_job(void *arg, async_t *co)
{
    fsm_ctx_t *c = (fsm_ctx_t *)arg;

    ASYNC_BEGIN(co);

top:
    /* ------------------------------------------------------------------
     * STATE_BOOT — run three init sub-coroutines sequentially.
     * ---------------------------------------------------------------- */
    c->state = STATE_BOOT;
    vibe_printk("[fsm] entering STATE_BOOT\n");

    /* Each async_init() resets the child's resume-point so it starts fresh,
     * even if this is a re-entry after a fault. */
    async_init(&c->hw_co);
    ASYNC_AWAIT_CORO(co, &c->hw_co, init_hw, &c->init_c);

    async_init(&c->periph_co);
    ASYNC_AWAIT_CORO(co, &c->periph_co, init_periph, &c->init_c);

    async_init(&c->app_co);
    ASYNC_AWAIT_CORO(co, &c->app_co, init_app, &c->init_c);

    vibe_printk("[fsm] boot complete — transitioning to OPERATIONAL\n");

    /* ------------------------------------------------------------------
     * STATE_OPERATIONAL — drive a nested scheduler with two concurrent
     * sub-jobs (heartbeat + sensor).
     * ---------------------------------------------------------------- */
    c->state         = STATE_OPERATIONAL;
    g_fault_detected = 0;

    /* Reset sub-job contexts for a clean start (important on re-entry). */
    c->hb_c.count     = 0;
    c->sensor_c.reading = 0;

    /* Initialise the nested operational scheduler and submit both jobs. */
    vibe_sched_init(&c->op_sched);
    vibe_sched_submit(&c->op_sched, heartbeat_job, &c->hb_c,     10U);
    vibe_sched_submit(&c->op_sched, sensor_job,    &c->sensor_c, 15U);

    vibe_printk("[fsm] entering STATE_OPERATIONAL\n");

    /* Drive the nested scheduler every tick until a fault is detected. */
    while (!g_fault_detected) {
        vibe_sched_run(&c->op_sched);
        ASYNC_YIELD(co); /* give the outer scheduler a turn */
    }

    /* ------------------------------------------------------------------
     * STATE_FAULT — log, wait, restart.
     * ---------------------------------------------------------------- */
    c->state = STATE_FAULT;
    c->fault_count++;
    vibe_printk("[fsm] entering STATE_FAULT (fault #%lu)\n",
                (unsigned long)c->fault_count);

    c->fault_deadline = vibe_tick_get() + VIBE_MS_TO_TICKS(2000);
    ASYNC_AWAIT(co, vibe_tick_get() >= c->fault_deadline);

    vibe_printk("[fsm] fault recovery complete — restarting boot sequence\n");

    /* Jump back to STATE_BOOT. async_t line numbers are set per-path, so
     * we use a plain goto to the top of the switch body.  This works
     * because goto labels inside ASYNC_BEGIN/END are valid C. */
    goto top;

    ASYNC_END(co);
}

/* =========================================================================
 * RTOS task — outer scheduler with a single FSM job.
 * ====================================================================== */

static vibe_job_sched_t g_sched;

static void fsm_task(void *arg)
{
    (void)arg;

    vibe_sched_init(&g_sched);
    vibe_sched_submit(&g_sched, fsm_job, &g_fsm, 10U);

    vibe_printk("[fsm] task started\n");

    for (;;) {
        vibe_sched_run(&g_sched);
        if (vibe_sched_is_idle(&g_sched)) {
            vibe_thread_yield();
        }
    }
}

/*
 * Stack: 2 KB — the FSM context embeds a nested vibe_job_sched_t
 * (CONFIG_JOB_SCHEDULER_MAX_JOBS × sizeof(vibe_job_t) bytes) in addition
 * to the normal call-frame overhead.
 */
VIBE_THREAD_DEFINE(fsm_thread, 2048, fsm_task, NULL, 16);

/* =========================================================================
 * main()
 * ====================================================================== */

int main(void)
{
    vibe_init();
    return 0;
}
