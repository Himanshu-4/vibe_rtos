/**
 * @file lib/async/async.h
 * @brief Stackless cooperative coroutines for VibeRTOS.
 *
 * Implements stackless coroutines using the Duff's-device line-tracking
 * technique (cf. Simon Tatham "Coroutines in C"; Adam Dunkels "Protothreads").
 * No additional stack is required per coroutine: the resume point is stored
 * in a small async_t state struct that lives in the caller's context.
 *
 * ---------------------------------------------------------------------------
 * USAGE
 * ---------------------------------------------------------------------------
 *
 * 1. Define a context struct that holds all state surviving a yield:
 *
 *      typedef struct { int i; uint32_t deadline; } my_ctx_t;
 *
 * 2. Write a coroutine function:
 *
 *      async_status_t counter(void *arg, async_t *co)
 *      {
 *          my_ctx_t *c = (my_ctx_t *)arg;
 *          ASYNC_BEGIN(co);
 *
 *          for (c->i = 0; c->i < 10; c->i++) {
 *              vibe_printk("tick %d\n", c->i);
 *              ASYNC_YIELD(co);                   // yield CPU, resume next tick
 *          }
 *
 *          ASYNC_AWAIT(co, sensor_ready());       // poll until condition
 *
 *          vibe_printk("sensor: %d\n", read_sensor());
 *
 *          ASYNC_END(co);
 *      }
 *
 * 3. Schedule it with the job scheduler:
 *
 *      static my_ctx_t ctx;
 *      vibe_sched_submit(&scheduler, counter, &ctx, 0);
 *
 * ---------------------------------------------------------------------------
 * LIMITATIONS (Duff's-device coroutines)
 * ---------------------------------------------------------------------------
 *  - Local variables declared between ASYNC_BEGIN / ASYNC_END are not
 *    preserved across a YIELD or AWAIT.  Store all persistent state in
 *    the context struct passed via @p arg.
 *  - ASYNC_YIELD / ASYNC_AWAIT must not be placed inside a switch()
 *    statement — they conflict with the outer switch opened by ASYNC_BEGIN.
 *  - The coroutine function must not be re-entered concurrently.
 *
 * @note CONFIG_ASYNC must be enabled (default y) for this header to be used.
 */

#ifndef VIBE_ASYNC_H
#define VIBE_ASYNC_H

/* -------------------------------------------------------------------------
 * Kconfig guard — fires when autoconf.h is present and ASYNC is disabled.
 * ---------------------------------------------------------------------- */
#if defined(CONFIG_ASYNC) && (CONFIG_ASYNC == 0)
#error "async.h: CONFIG_ASYNC is disabled. " \
       "Enable it via menuconfig → Libraries → Async / Coroutines."
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Coroutine status
 * ---------------------------------------------------------------------- */

/**
 * Return value of every async coroutine function.
 */
typedef enum {
    ASYNC_DONE    = 0,  /**< Coroutine has completed; must not be resumed. */
    ASYNC_RUNNING = 1,  /**< Yielded normally; will be resumed next tick.   */
    ASYNC_BLOCKED = 2,  /**< Waiting for a condition; re-probe next tick.   */
} async_status_t;

/* -------------------------------------------------------------------------
 * Coroutine state
 * ---------------------------------------------------------------------- */

/**
 * Per-coroutine-instance state.  One of these must exist for every active
 * coroutine invocation (typically embedded in the job context struct or
 * declared alongside it).
 */
typedef struct {
    int line;   /**< Resume-point line number.  0 = not started, -1 = done. */
} async_t;

/**
 * Coroutine function type.
 *
 * @param ctx  User-supplied context pointer (persistent state).
 * @param co   Coroutine state (resume point).
 * @return     ASYNC_DONE / ASYNC_RUNNING / ASYNC_BLOCKED.
 */
typedef async_status_t (*async_fn_t)(void *ctx, async_t *co);

/** Initialise or reset a coroutine so it starts from the beginning. */
static inline void async_init(async_t *co) { co->line = 0; }

/** True once a coroutine has returned ASYNC_DONE. */
static inline int  async_is_done(const async_t *co) { return co->line < 0; }

/* -------------------------------------------------------------------------
 * Coroutine control macros
 * ---------------------------------------------------------------------- */

/**
 * ASYNC_BEGIN — open the coroutine body.
 *
 * Must appear as the first statement inside an async function, before
 * any code that should participate in the state machine.
 */
#define ASYNC_BEGIN(co)     switch ((co)->line) { case 0:

/**
 * ASYNC_END — close the coroutine body and mark it done.
 *
 * Must appear as the last statement (immediately before the closing brace).
 * After this returns ASYNC_DONE the scheduler will not invoke the coroutine
 * again unless async_init() is called to reset its state.
 */
#define ASYNC_END(co)       } (co)->line = -1; return ASYNC_DONE

/**
 * ASYNC_YIELD — give up the CPU and resume on the next scheduler tick.
 *
 * Use when you want to run periodically without waiting for a condition.
 * All state to be preserved must be in the context struct (@p arg).
 */
#define ASYNC_YIELD(co)                                                 \
    do {                                                                \
        (co)->line = __LINE__;  return ASYNC_RUNNING;                   \
        case __LINE__:;                                                 \
    } while (0)

/**
 * ASYNC_AWAIT — suspend until @p cond becomes true, yielding every tick.
 *
 * The scheduler re-invokes the coroutine each tick to re-evaluate the
 * condition (polling).  While waiting the coroutine returns ASYNC_BLOCKED,
 * which the scheduler uses to distinguish it from actively-running jobs.
 *
 * @param co    Coroutine state.
 * @param cond  Boolean expression, re-evaluated every scheduler tick.
 */
#define ASYNC_AWAIT(co, cond)                                           \
    do {                                                                \
        (co)->line = __LINE__;                                          \
        __attribute__((fallthrough));                                   \
        case __LINE__:                                                  \
        if (!(cond)) { return ASYNC_BLOCKED; }                          \
    } while (0)

/**
 * ASYNC_AWAIT_CORO — suspend until a child coroutine finishes.
 *
 * The parent coroutine resumes (and re-invokes the child) every scheduler
 * tick until the child returns ASYNC_DONE.
 *
 * The child's async_t (@p child_co) must be initialised with async_init()
 * before calling this macro.
 *
 * Example:
 *   static async_t child_state;
 *   async_init(&child_state);
 *   ASYNC_AWAIT_CORO(co, &child_state, child_fn, child_ctx);
 *
 * @param co        Parent coroutine state.
 * @param child_co  Child coroutine state (async_t *).
 * @param fn        Child coroutine function (async_fn_t).
 * @param ctx       Context pointer passed to the child.
 */
#define ASYNC_AWAIT_CORO(co, child_co, fn, ctx)                         \
    do {                                                                \
        (co)->line = __LINE__;                                          \
        __attribute__((fallthrough));                                   \
        case __LINE__:                                                  \
        if ((fn)((ctx), (child_co)) != ASYNC_DONE) {                   \
            return ASYNC_RUNNING;                                       \
        }                                                               \
    } while (0)

/**
 * ASYNC_RETURN — return early from a coroutine, marking it as done.
 */
#define ASYNC_RETURN(co)    do { (co)->line = -1; return ASYNC_DONE; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* VIBE_ASYNC_H */
