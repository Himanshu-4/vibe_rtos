#ifndef DRIVERS_I2C_H
#define DRIVERS_I2C_H

#include "vibe/types.h"
#include "vibe/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_MSG_READ   BIT(0)
#define I2C_MSG_STOP   BIT(1)

typedef struct {
    uint8_t *buf;
    size_t   len;
    uint8_t  flags;  /**< I2C_MSG_READ, I2C_MSG_STOP */
} i2c_msg_t;

typedef struct i2c_ops {
    vibe_device_api_t base;
    vibe_err_t (*transfer)(const vibe_device_t *dev,
                            i2c_msg_t *msgs, uint8_t num_msgs,
                            uint16_t addr);
} i2c_ops_t;

static inline vibe_err_t i2c_transfer(const vibe_device_t *dev,
                                        i2c_msg_t *msgs, uint8_t num_msgs,
                                        uint16_t addr)
{
    return ((const i2c_ops_t *)dev->api)->transfer(dev, msgs, num_msgs, addr);
}

static inline vibe_err_t i2c_write(const vibe_device_t *dev,
                                     const uint8_t *buf, size_t len,
                                     uint16_t addr)
{
    i2c_msg_t msg = { .buf = (uint8_t *)buf, .len = len,
                       .flags = I2C_MSG_STOP };
    return i2c_transfer(dev, &msg, 1, addr);
}

static inline vibe_err_t i2c_read(const vibe_device_t *dev,
                                    uint8_t *buf, size_t len, uint16_t addr)
{
    i2c_msg_t msg = { .buf = buf, .len = len,
                       .flags = I2C_MSG_READ | I2C_MSG_STOP };
    return i2c_transfer(dev, &msg, 1, addr);
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_I2C_H */
