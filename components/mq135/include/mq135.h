#pragma once
/**
 * @file mq135.h
 * @brief MQ135 Air Quality Sensor Driver (ADC)
 *
 * MQ135 outputs analog voltage proportional to gas concentration.
 * We read via ADC and return raw value + rough AQI percentage.
 *
 * Note: MQ135 requires 24-48h burn-in for accurate readings.
 * This driver provides raw ADC values; calibration against
 * known gas concentrations is application-specific.
 */

#include <stdint.h>
#include "esp_err.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    adc_unit_t    unit;
    adc_channel_t channel;
    adc_atten_t   atten;
} mq135_handle_t;

/**
 * @brief Initialize MQ135 ADC reading
 * @param handle    Output handle
 * @param unit      ADC unit (ADC_UNIT_1 or ADC_UNIT_2)
 * @param channel   ADC channel
 * @param atten     Attenuation (ADC_ATTEN_DB_12 for full 0-3.3V range)
 * @return ESP_OK on success
 */
esp_err_t mq135_init(mq135_handle_t *handle,
                     adc_unit_t unit,
                     adc_channel_t channel,
                     adc_atten_t atten);

/**
 * @brief Read raw ADC value from MQ135
 * @param handle    Handle from mq135_init()
 * @param raw       Output: raw 12-bit ADC value (0-4095)
 * @return ESP_OK on success
 */
esp_err_t mq135_read_raw(mq135_handle_t *handle, uint16_t *raw);

/**
 * @brief Read and convert to approximate AQI percentage (0-100)
 * @param handle    Handle from mq135_init()
 * @param aqi_pct   Output: AQI percentage (higher = worse air)
 * @return ESP_OK on success
 */
esp_err_t mq135_read_aqi(mq135_handle_t *handle, uint16_t *aqi_pct);

#ifdef __cplusplus
}
#endif
