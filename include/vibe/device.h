#ifndef VIBE_DEVICE_H
#define VIBE_DEVICE_H

/**
 * @file device.h
 * @brief Device model for VibeRTOS.
 *
 * Provides a uniform device registration and lookup mechanism with
 * ordered initialisation levels and power management callbacks.
 *
 * Devices are placed in a named linker section (._vibe_devices) and
 * iterated at boot in priority order by vibe_init().
 */

#include <stddef.h>
#include <stdbool.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Initialisation levels
 * --------------------------------------------------------------------- */

/**
 * Device initialisation ordering.
 * Devices with lower level values are initialised first.
 */
typedef enum {
    VIBE_INIT_PRE_KERNEL_1 = 0, /**< Very early hardware init (clocks, etc.). */
    VIBE_INIT_PRE_KERNEL_2 = 1, /**< Early init that needs PRE_KERNEL_1 done. */
    VIBE_INIT_POST_KERNEL  = 2, /**< Kernel services available. */
    VIBE_INIT_APPLICATION  = 3, /**< Application-level init. */
} vibe_init_level_t;

/* -----------------------------------------------------------------------
 * Power management ops
 * --------------------------------------------------------------------- */

typedef struct vibe_pm_ops {
    /** Suspend the device (called before entering low-power state). */
    vibe_err_t (*suspend)(const struct vibe_device *dev);
    /** Resume the device (called after waking from low-power state). */
    vibe_err_t (*resume)(const struct vibe_device *dev);
} vibe_pm_ops_t;

/* -----------------------------------------------------------------------
 * Generic device API structure
 * --------------------------------------------------------------------- */

/**
 * Generic driver operations table.
 * Concrete drivers embed their specific ops alongside or extend this.
 */
typedef struct vibe_device_api {
    const char *type;  /**< Driver type string, e.g. "uart", "gpio". */
} vibe_device_api_t;

/* -----------------------------------------------------------------------
 * Device descriptor
 * --------------------------------------------------------------------- */

typedef struct vibe_device {
    const char              *name;     /**< Human-readable device name. */
    const vibe_device_api_t *api;      /**< Pointer to driver operations table. */
    const void              *config;   /**< Immutable driver configuration (const). */
    void                    *data;     /**< Mutable driver state (runtime data). */
    vibe_err_t             (*init_fn)(const struct vibe_device *dev); /**< Init function. */
    const vibe_pm_ops_t     *pm_ops;   /**< Power management ops, or NULL. */
} vibe_device_t;

/* -----------------------------------------------------------------------
 * Device registration macro
 * --------------------------------------------------------------------- */

/**
 * VIBE_DEVICE_DEFINE — register a device in the linker section.
 *
 * The device is placed in the ._vibe_devices.<level> section so that the
 * linker script can group devices by init level. vibe_init() iterates
 * these sections in order.
 *
 * @param dev_name  String name for device lookup.
 * @param init_fn_  Init function of type vibe_err_t (*)(const vibe_device_t *).
 * @param config_   Pointer to const driver config struct.
 * @param data_     Pointer to mutable driver data struct.
 * @param api_      Pointer to vibe_device_api_t (or derived ops struct).
 * @param level     vibe_init_level_t ordering.
 * @param pm_ops_   Pointer to vibe_pm_ops_t, or NULL.
 */
#define VIBE_DEVICE_DEFINE(dev_name, init_fn_, config_, data_, api_, level, pm_ops_) \
    static const vibe_device_t _vibe_dev_##dev_name                                   \
        __attribute__((used,                                                           \
                       section("._vibe_devices." #level))) = {                        \
        .name    = #dev_name,                                                          \
        .api     = (const vibe_device_api_t *)(api_),                                 \
        .config  = (config_),                                                          \
        .data    = (data_),                                                            \
        .init_fn = (init_fn_),                                                         \
        .pm_ops  = (pm_ops_),                                                          \
    }

/* -----------------------------------------------------------------------
 * Device lookup
 * --------------------------------------------------------------------- */

/**
 * @brief Look up a registered device by name.
 *
 * Searches the ._vibe_devices linker sections for a device whose name
 * field matches the given string.
 *
 * @param name  Device name string (as given to VIBE_DEVICE_DEFINE).
 * @return      Pointer to vibe_device_t, or NULL if not found.
 */
const vibe_device_t *vibe_device_get(const char *name);

/**
 * @brief Check whether a device has been successfully initialised.
 *
 * @param dev  Device to query.
 * @return     true if the device's init_fn returned VIBE_OK.
 */
bool vibe_device_is_ready(const vibe_device_t *dev);

/**
 * @brief Initialise all devices up to and including the given level.
 *
 * Called internally by vibe_init(). Applications should not call this directly.
 *
 * @param level  Maximum init level to run.
 */
void _vibe_device_init_level(vibe_init_level_t level);

/* -----------------------------------------------------------------------
 * Linker section symbols (defined in linker script)
 * --------------------------------------------------------------------- */

extern const vibe_device_t _vibe_devices_start[];
extern const vibe_device_t _vibe_devices_end[];

#ifdef __cplusplus
}
#endif

#endif /* VIBE_DEVICE_H */
