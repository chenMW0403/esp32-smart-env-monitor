/**
 * @file bh1750.c
 * @brief BH1750 Driver Implementation (ESP-IDF I2C Master API)
 *
 * Uses the new I2C master driver (v5.x API).
 */

#include "bh1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BH1750";

/* BH1750 opcodes */
#define BH1750_POWER_ON         0x01
#define BH1750_RESET            0x07
#define BH1750_CONT_H_RES_MODE  0x10    /* Continuously H-Res Mode, 1 lx, 120ms */

esp_err_t bh1750_init(bh1750_handle_t *handle,
                      i2c_master_bus_handle_t bus,
                      uint16_t addr)
{
    handle->bus  = bus;
    handle->addr = addr;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 100000,  /* 100 kHz */
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &handle->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Power on */
    uint8_t cmd = BH1750_POWER_ON;
    ret = i2c_master_transmit(handle->dev, &cmd, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power-on command failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set to continuously high-resolution mode */
    cmd = BH1750_CONT_H_RES_MODE;
    ret = i2c_master_transmit(handle->dev, &cmd, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "BH1750 initialized at addr 0x%02X", addr);
    return ESP_OK;
}

esp_err_t bh1750_read_lux(bh1750_handle_t *handle, float *lux)
{
    uint8_t buf[2] = {0};

    esp_err_t ret = i2c_master_receive(handle->dev, buf, 2, 200);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];

    /* Conversion: lux = raw / 1.2 */
    *lux = (float)raw / 1.2f;

    ESP_LOGD(TAG, "Raw=0x%04X  Lux=%.1f", raw, *lux);
    return ESP_OK;
}
