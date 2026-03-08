/**
 * @file drivers/flash/flash.c
 * @brief Flash driver stub for VibeRTOS.
 * USER: implement using ROM flash functions (RP2040) or XIP SSI directly.
 *
 * RP2040 provides flash_range_erase() and flash_range_program() in ROM.
 * These must be called from RAM with XIP/flash cache disabled.
 */

#include "drivers/flash.h"
#include "vibe/device.h"
#include <stddef.h>

#ifdef CONFIG_FLASH

static vibe_err_t _flash_read(const vibe_device_t *dev, off_t offset,
                                void *buf, size_t len)
{
    (void)dev;
    /* On RP2040 flash is memory-mapped at 0x10000000 — just memcpy. */
    const uint8_t *flash_base = (const uint8_t *)0x10000000U;
    __builtin_memcpy(buf, flash_base + offset, len);
    return VIBE_OK;
}

static vibe_err_t _flash_write(const vibe_device_t *dev, off_t offset,
                                 const void *buf, size_t len)
{
    (void)dev; (void)offset; (void)buf; (void)len;
    /* TODO: call ROM flash_range_program() from RAM-resident code */
    return VIBE_ENOSYS;
}

static vibe_err_t _flash_erase_page(const vibe_device_t *dev, off_t offset)
{
    (void)dev; (void)offset;
    /* TODO: call ROM flash_range_erase() from RAM-resident code */
    return VIBE_ENOSYS;
}

static vibe_err_t _flash_erase_sector(const vibe_device_t *dev, off_t offset)
{
    return _flash_erase_page(dev, offset);
}

static size_t _flash_page_size(const vibe_device_t *dev)
{
    (void)dev;
    return 4096U; /* RP2040 sector/page = 4KB */
}

static vibe_err_t _flash_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const flash_ops_t _flash0_ops = {
    .base         = { .type = "flash" },
    .read         = _flash_read,
    .write        = _flash_write,
    .erase_page   = _flash_erase_page,
    .erase_sector = _flash_erase_sector,
    .page_size    = _flash_page_size,
};

VIBE_DEVICE_DEFINE(flash0, _flash_init, NULL, NULL,
                   &_flash0_ops, VIBE_INIT_PRE_KERNEL_2, NULL);

#endif /* CONFIG_FLASH */
