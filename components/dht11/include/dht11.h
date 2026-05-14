#pragma once
/**
 * @file dht11.h
 * @brief DHT11 Temperature & Humidity Sensor Driver
 *
 * Single-wire protocol on GPIO. Timing-critical implementation
 * using esp_timer for microsecond precision.
 */

#include <stdint.h>
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;  /* °C */
    float humidity;     /* %RH */
} dht11_data_t;

/**
 * @brief Initialize DHT11 sensor on given GPIO pin
 * @param gpio_pin  GPIO pin number connected to DHT11 DATA
 * @return ESP_OK on success
 */
esp_err_t dht11_init(gpio_num_t gpio_pin);

/**
 * @brief Read temperature and humidity from DHT11
 * @param data  Output: temperature and humidity
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if sensor not responding,
 *         ESP_ERR_INVALID_CRC on checksum error
 *
 * Note: DHT11 has 1s minimum sampling interval. Calling more frequently
 * may return stale or invalid data.
 */
esp_err_t dht11_read(dht11_data_t *data);

#ifdef __cplusplus
}
#endif
