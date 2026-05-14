/**
 * @file mq135.c
 * @brief MQ135 ADC Driver Implementation
 */

#include "mq135.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "MQ135";

esp_err_t mq135_init(mq135_handle_t *handle,
                     adc_unit_t unit,
                     adc_channel_t channel,
                     adc_atten_t atten)
{
    handle->unit    = unit;
    handle->channel = channel;
    handle->atten   = atten;

    ESP_LOGI(TAG, "MQ135 initialized: ADC%d_CH%d, atten=%d", unit, channel, atten);
    return ESP_OK;
}

static esp_err_t read_adc_raw(adc_unit_t unit, adc_channel_t channel,
                               adc_atten_t atten, int *out_raw)
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = unit,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &adc_handle);
    if (ret != ESP_OK) return ret;

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = atten,
    };
    ret = adc_oneshot_config_channel(adc_handle, channel, &chan_cfg);
    if (ret != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        return ret;
    }

    ret = adc_oneshot_read(adc_handle, channel, out_raw);

    adc_oneshot_del_unit(adc_handle);
    return ret;
}

esp_err_t mq135_read_raw(mq135_handle_t *handle, uint16_t *raw)
{
    int val = 0;
    esp_err_t ret = read_adc_raw(handle->unit, handle->channel,
                                  handle->atten, &val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return ret;
    }
    *raw = (uint16_t)val;
    ESP_LOGD(TAG, "Raw ADC = %d", val);
    return ESP_OK;
}

esp_err_t mq135_read_aqi(mq135_handle_t *handle, uint16_t *aqi_pct)
{
    uint16_t raw;
    esp_err_t ret = mq135_read_raw(handle, &raw);
    if (ret != ESP_OK) return ret;

    /* Simple linear mapping: 0-4095 -> 0-100%
     * This is a rough approximation. Real calibration requires
     * exposure to known gas concentrations. */
    *aqi_pct = (uint16_t)(((float)raw / 4095.0f) * 100.0f);
    if (*aqi_pct > 100) *aqi_pct = 100;

    ESP_LOGD(TAG, "AQI = %d%%", *aqi_pct);
    return ESP_OK;
}
