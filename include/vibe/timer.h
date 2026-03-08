#ifndef VIBE_TIMER_H
#define VIBE_TIMER_H

/**
 * @file timer.h
 * @brief Software timer API for VibeRTOS.
 *
 * Software timers are driven by the system tick. Timer callbacks execute
 * in ISR context (tick handler) by default, or can be deferred to the
 * system work queue for safer execution in thread context.
 */

#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Timer mode
 * --------------------------------------------------------------------- */

typedef enum {
    VIBE_TIMER_ONE_SHOT = 0,  /**< Fire once then stop. */
    VIBE_TIMER_PERIODIC = 1,  /**< Reload and fire repeatedly. */
} vibe_timer_mode_t;

/* -----------------------------------------------------------------------
 * Timer callback type
 * --------------------------------------------------------------------- */

struct vibe_timer;

/** Timer expiry callback. Called with a pointer to the timer that fired. */
typedef void (*vibe_timer_expiry_fn_t)(struct vibe_timer *timer, void *user_data);

/* -----------------------------------------------------------------------
 * Timer struct
 * --------------------------------------------------------------------- */

typedef struct vibe_timer {
    vibe_timer_expiry_fn_t  expiry_fn;        /**< Function called on expiry. */
    void                   *user_data;        /**< Opaque pointer passed to expiry_fn. */
    vibe_tick_t             duration_ticks;   /**< Reload value (periodic) or initial value. */
    vibe_tick_t             remaining;        /**< Ticks until next expiry. */
    vibe_timer_mode_t       mode;             /**< ONE_SHOT or PERIODIC. */
    bool                    active;           /**< Whether the timer is currently running. */
    struct vibe_timer      *next;             /**< Intrusive link in the timer list. */
} vibe_timer_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_TIMER_DEFINE — statically define a software timer.
 *
 * @param name   C identifier.
 * @param fn     Expiry callback of type vibe_timer_expiry_fn_t.
 * @param tmode  VIBE_TIMER_ONE_SHOT or VIBE_TIMER_PERIODIC.
 */
#define VIBE_TIMER_DEFINE(name, fn, tmode)   \
    vibe_timer_t name = {                    \
        .expiry_fn      = (fn),              \
        .user_data      = NULL,              \
        .duration_ticks = 0U,                \
        .remaining      = 0U,               \
        .mode           = (tmode),           \
        .active         = false,             \
        .next           = NULL,              \
    }

/* -----------------------------------------------------------------------
 * Timer API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a timer.
 *
 * @param timer      Timer to initialise.
 * @param expiry_fn  Callback invoked when the timer expires.
 * @param user_data  Opaque pointer passed to expiry_fn.
 * @param mode       ONE_SHOT or PERIODIC.
 * @return           VIBE_OK.
 */
vibe_err_t vibe_timer_init(vibe_timer_t          *timer,
                            vibe_timer_expiry_fn_t expiry_fn,
                            void                  *user_data,
                            vibe_timer_mode_t      mode);

/**
 * @brief Start a timer with the given duration.
 *
 * If the timer is already running, it is restarted.
 *
 * @param timer       Timer to start.
 * @param duration_ms Duration in milliseconds.
 * @return            VIBE_OK.
 */
vibe_err_t vibe_timer_start(vibe_timer_t *timer, uint32_t duration_ms);

/**
 * @brief Stop a running timer.
 *
 * The timer is removed from the active list; its callback will not fire.
 *
 * @param timer  Timer to stop.
 * @return       VIBE_OK, or VIBE_EAGAIN if already stopped.
 */
vibe_err_t vibe_timer_stop(vibe_timer_t *timer);

/**
 * @brief Restart a timer with its original duration.
 *
 * Equivalent to stop + start with the previously configured duration.
 *
 * @param timer  Timer to restart.
 * @return       VIBE_OK.
 */
vibe_err_t vibe_timer_restart(vibe_timer_t *timer);

/**
 * @brief Check whether a timer is currently running.
 *
 * @param timer  Timer to query.
 * @return       true if running.
 */
bool vibe_timer_is_running(const vibe_timer_t *timer);

/**
 * @brief Return the remaining time until the timer fires.
 *
 * @param timer  Timer to query.
 * @return       Remaining milliseconds, or 0 if stopped.
 */
uint32_t vibe_timer_remaining_ms(const vibe_timer_t *timer);

/* -----------------------------------------------------------------------
 * Internal: called from the system tick handler
 * --------------------------------------------------------------------- */

/**
 * @brief Advance all active timers by one tick.
 *
 * Called from arch_systick_handler() or the kernel tick ISR.
 * Fires expired timers and removes ONE_SHOT timers from the list.
 * Updates the tickless-idle next-wakeup calculation.
 */
void _vibe_timer_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_TIMER_H */
