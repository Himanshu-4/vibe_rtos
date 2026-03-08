#ifndef VIBE_EVENT_H
#define VIBE_EVENT_H

/**
 * @file event.h
 * @brief Event flags / event group API for VibeRTOS.
 *
 * Provides a 32-bit event word where each bit represents an independent
 * event flag. Threads can wait for any combination of bits using
 * WAIT_ANY (at least one) or WAIT_ALL (all requested bits) semantics.
 */

#include "vibe/types.h"
#include "vibe/sys/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Event wait mode
 * --------------------------------------------------------------------- */

typedef enum {
    VIBE_EVENT_WAIT_ANY = 0,  /**< Wake when ANY of the requested bits are set. */
    VIBE_EVENT_WAIT_ALL = 1,  /**< Wake when ALL requested bits are set. */
} vibe_event_wait_mode_t;

/* -----------------------------------------------------------------------
 * Event struct
 * --------------------------------------------------------------------- */

typedef struct vibe_event {
    uint32_t    flags;      /**< Current set of event flags. */
    vibe_list_t wait_list;  /**< Threads waiting on this event. */
} vibe_event_t;

/* -----------------------------------------------------------------------
 * Static definition macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_EVENT_DEFINE — statically define and initialise an event group.
 *
 * @param name  C identifier for the event variable.
 */
#define VIBE_EVENT_DEFINE(name) \
    vibe_event_t name = {       \
        .flags = 0U,            \
    }

/* -----------------------------------------------------------------------
 * Event API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise an event group.
 *
 * @param event  Pointer to vibe_event_t to initialise.
 * @return       VIBE_OK.
 */
vibe_err_t vibe_event_init(vibe_event_t *event);

/**
 * @brief Post (set) one or more event flags.
 *
 * Sets the specified bits and wakes any waiters whose conditions are met.
 * Safe to call from ISR context.
 *
 * @param event  Event group.
 * @param flags  Bitmask of flags to set.
 * @return       VIBE_OK.
 */
vibe_err_t vibe_event_post(vibe_event_t *event, uint32_t flags);

/**
 * @brief Clear one or more event flags.
 *
 * @param event  Event group.
 * @param flags  Bitmask of flags to clear.
 * @return       VIBE_OK.
 */
vibe_err_t vibe_event_clear(vibe_event_t *event, uint32_t flags);

/**
 * @brief Wait for event flags, blocking until the condition is met.
 *
 * @param event    Event group.
 * @param flags    Bitmask of flags to wait for.
 * @param mode     VIBE_EVENT_WAIT_ANY or VIBE_EVENT_WAIT_ALL.
 * @param timeout  VIBE_WAIT_FOREVER, VIBE_NO_WAIT, or tick count.
 * @return         VIBE_OK on success, VIBE_ETIMEOUT, VIBE_EAGAIN.
 */
vibe_err_t vibe_event_wait(vibe_event_t          *event,
                            uint32_t               flags,
                            vibe_event_wait_mode_t mode,
                            vibe_tick_t            timeout);

/**
 * @brief Return the current event flags without blocking.
 *
 * @param event  Event group.
 * @return       Current 32-bit flags word.
 */
uint32_t vibe_event_get(const vibe_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_EVENT_H */
