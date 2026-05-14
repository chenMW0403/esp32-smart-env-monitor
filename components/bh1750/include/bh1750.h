#pragma once
/**
 * @file bh1750.h
 * @brief BH1750FVI Ambient Light Sensor Driver (I2C)
 *
 * Resolution: 1 - 65535 lux
 * Interface: I2C (address 0x23 or 0x5C)
 * Modes: Continuously H-Resolution (1 lx, 120ms)
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    uint16_t                addr;
} bh1750_handle_t;

/**
 * @brief Initialize BH1750 on an existing I2C master bus
 * @param handle    Output device handle
 * @param bus       I2C master bus handle (already created)
 * @param addr      I2C address (0x23 default, 0x5C if ADDR pin HIGH)
 * @return ESP_OK on success
 */
esp_err_t bh1750_init(bh1750_handle_t *handle,
                      i2c_master_bus_handle_t bus,
                      uint16_t addr);

/**
 * @brief Read ambient light intensity
 * @param handle    Device handle from bh1750_init()
 * @param lux       Output: light intensity in lux
 * @return ESP_OK on success
 */
esp_err_t bh1750_read_lux(bh1750_handle_t *handle, float *lux);

#ifdef __cplusplus
}
#endif
