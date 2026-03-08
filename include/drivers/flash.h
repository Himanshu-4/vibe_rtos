#ifndef DRIVERS_FLASH_H
#define DRIVERS_FLASH_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flash_ops {
    vibe_device_api_t base;
    vibe_err_t (*read)       (const vibe_device_t *dev, off_t offset,
                               void *buf, size_t len);
    vibe_err_t (*write)      (const vibe_device_t *dev, off_t offset,
                               const void *buf, size_t len);
    vibe_err_t (*erase_page) (const vibe_device_t *dev, off_t offset);
    vibe_err_t (*erase_sector)(const vibe_device_t *dev, off_t offset);
    size_t     (*page_size)  (const vibe_device_t *dev);
} flash_ops_t;

static inline vibe_err_t flash_read(const vibe_device_t *dev,
                                     off_t offset, void *buf, size_t len)
{ return ((const flash_ops_t *)dev->api)->read(dev, offset, buf, len); }

static inline vibe_err_t flash_write(const vibe_device_t *dev,
                                      off_t offset, const void *buf, size_t len)
{ return ((const flash_ops_t *)dev->api)->write(dev, offset, buf, len); }

static inline vibe_err_t flash_erase_page(const vibe_device_t *dev, off_t offset)
{ return ((const flash_ops_t *)dev->api)->erase_page(dev, offset); }

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_FLASH_H */
