/**
 * @file kernel/timer.c
 * @brief Software timer management for VibeRTOS.
 *
 * Maintains a singly-linked list of active timers, sorted by remaining
 * tick count. _vibe_timer_tick() is called from the system tick ISR and
 * decrements/fires timers.
 */

#include "vibe/timer.h"
#include "vibe/types.h"
#include "vibe/workq.h"
#include "vibe/sys/list.h"
#include "vibe/sys/printk.h"
#include <string.h>

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

/* -----------------------------------------------------------------------
 * Active timer list (singly-linked, sorted by remaining descending)
 * --------------------------------------------------------------------- */

static vibe_timer_t *g_timer_list_head = NULL;

/* -----------------------------------------------------------------------
 * Timer init
 * --------------------------------------------------------------------- */

vibe_err_t vibe_timer_init(vibe_timer_t          *timer,
                            vibe_timer_expiry_fn_t expiry_fn,
                            void                  *user_data,
                            vibe_timer_mode_t      mode)
{
    if (timer == NULL || expiry_fn == NULL) {
        return VIBE_EINVAL;
    }

    memset(timer, 0, sizeof(*timer));
    timer->expiry_fn  = expiry_fn;
    timer->user_data  = user_data;
    timer->mode       = mode;
    timer->active     = false;
    timer->next       = NULL;

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Timer start
 * --------------------------------------------------------------------- */

vibe_err_t vibe_timer_start(vibe_timer_t *timer, uint32_t duration_ms)
{
    if (timer == NULL || duration_ms == 0U) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    /* If already active, remove and re-add. */
    if (timer->active) {
        vibe_timer_stop(timer);
    }

    timer->duration_ticks = VIBE_MS_TO_TICKS(duration_ms);
    if (timer->duration_ticks == 0U) {
        timer->duration_ticks = 1U;
    }
    timer->remaining = timer->duration_ticks;
    timer->active    = true;

    /* Insert at head of list (simple — not sorted by remaining). */
    timer->next          = g_timer_list_head;
    g_timer_list_head    = timer;

    arch_irq_unlock(key);

#ifdef CONFIG_TICKLESS_IDLE
    /* Notify arch that next wakeup may have moved closer. */
    extern vibe_tick_t _vibe_sched_next_wakeup(void);
    vibe_tick_t next = _vibe_sched_next_wakeup();
    if (next != VIBE_WAIT_FOREVER) {
        arch_set_next_tick(next);
    }
#endif

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Timer stop (must be called with IRQs locked or atomically)
 * --------------------------------------------------------------------- */

vibe_err_t vibe_timer_stop(vibe_timer_t *timer)
{
    if (timer == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (!timer->active) {
        arch_irq_unlock(key);
        return VIBE_EAGAIN;
    }

    /* Remove from singly-linked list. */
    vibe_timer_t **pp = &g_timer_list_head;
    while (*pp != NULL) {
        if (*pp == timer) {
            *pp = timer->next;
            break;
        }
        pp = &(*pp)->next;
    }

    timer->active    = false;
    timer->remaining = 0U;
    timer->next      = NULL;

    arch_irq_unlock(key);
    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Timer restart
 * --------------------------------------------------------------------- */

vibe_err_t vibe_timer_restart(vibe_timer_t *timer)
{
    if (timer == NULL) {
        return VIBE_EINVAL;
    }
    uint32_t ms = VIBE_TICKS_TO_MS(timer->duration_ticks);
    return vibe_timer_start(timer, ms);
}

/* -----------------------------------------------------------------------
 * Queries
 * --------------------------------------------------------------------- */

bool vibe_timer_is_running(const vibe_timer_t *timer)
{
    return timer != NULL && timer->active;
}

uint32_t vibe_timer_remaining_ms(const vibe_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return 0U;
    }
    return VIBE_TICKS_TO_MS(timer->remaining);
}

/* -----------------------------------------------------------------------
 * _vibe_timer_tick() — called from SysTick ISR every tick
 * --------------------------------------------------------------------- */

void _vibe_timer_tick(void)
{
    vibe_timer_t *t    = g_timer_list_head;
    vibe_timer_t *prev = NULL;

    while (t != NULL) {
        vibe_timer_t *next = t->next;

        if (t->remaining > 0U) {
            t->remaining--;
        }

        if (t->remaining == 0U && t->active) {
            /* Fire the timer. */
            if (t->mode == VIBE_TIMER_PERIODIC) {
                /* Reload for next period. */
                t->remaining = t->duration_ticks;
                prev = t;
            } else {
                /* ONE_SHOT: remove from list. */
                if (prev != NULL) {
                    prev->next = next;
                } else {
                    g_timer_list_head = next;
                }
                t->active = false;
                t->next   = NULL;
            }

            /* Call the user callback.
             * NOTE: runs in ISR context. Keep callbacks very short.
             * For longer work, submit to vibe_sys_workq inside the callback. */
            if (t->expiry_fn != NULL) {
                t->expiry_fn(t, t->user_data);
            }
        } else {
            prev = t;
        }

        t = next;
    }
}
