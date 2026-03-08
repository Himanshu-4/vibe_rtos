/**
 * @file drivers/i2c/i2c.c
 * @brief I2C driver stub for VibeRTOS.
 * USER: implement with RP2040 I2C0/I2C1 (DW_apb_i2c IP).
 */

#include "drivers/i2c.h"
#include "vibe/device.h"

#ifdef CONFIG_I2C

static vibe_err_t _i2c_transfer(const vibe_device_t *dev,
                                  i2c_msg_t *msgs, uint8_t num_msgs,
                                  uint16_t addr)
{
    (void)dev; (void)msgs; (void)num_msgs; (void)addr;
    /* TODO: implement DesignWare I2C transfer sequence */
    return VIBE_ENOSYS;
}

static vibe_err_t _i2c_init(const vibe_device_t *dev) { (void)dev; return VIBE_OK; }

static const i2c_ops_t _i2c0_ops = {
    .base     = { .type = "i2c" },
    .transfer = _i2c_transfer,
};

VIBE_DEVICE_DEFINE(i2c0, _i2c_init, NULL, NULL,
                   &_i2c0_ops, VIBE_INIT_POST_KERNEL, NULL);

#endif /* CONFIG_I2C */
