/**
 * @file dht11.c
 * @brief DHT11 Driver Implementation
 *
 * Protocol: Host pulls LOW >= 18ms, then releases (HIGH).
 * Sensor responds: 80us LOW + 80us HIGH (ack).
 * Data bits: 5 x 8 bits = 40 bits total.
 *   - '0': 50us LOW + 26-28us HIGH
 *   - '1': 50us LOW + 70us HIGH
 *
 * Uses esp_timer for microsecond timing and portMUX for
 * critical sections during bit-bang.
 */

#include "dht11.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";
static gpio_num_t s_gpio = GPIO_NUM_NC;
static portMUX_TYPE s_dht_mux = portMUX_INITIALIZER_UNLOCKED;

/* ---- Helpers ---- */

static inline void delay_us(uint32_t us)
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (int64_t)us) {
        /* busy-wait */
    }
}

static void set_output(void)
{
    gpio_set_direction(s_gpio, GPIO_MODE_OUTPUT);
}

static void set_input(void)
{
    gpio_set_direction(s_gpio, GPIO_MODE_INPUT);
}

static int wait_level(int level, uint32_t timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_gpio) != level) {
        if ((esp_timer_get_time() - start) > (int64_t)timeout_us) {
            return -1;
        }
    }
    return 0;
}

/* ---- Public API ---- */

esp_err_t dht11_init(gpio_num_t gpio_pin)
{
    s_gpio = gpio_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(s_gpio, 1);
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", gpio_pin);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    if (s_gpio == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[5] = {0};

    /* ---- Start signal: pull LOW >= 18ms ---- */
    taskENTER_CRITICAL(&s_dht_mux);
    set_output();
    gpio_set_level(s_gpio, 0);
    taskEXIT_CRITICAL(&s_dht_mux);

    vTaskDelay(pdMS_TO_TICKS(20));

    taskENTER_CRITICAL(&s_dht_mux);
    gpio_set_level(s_gpio, 1);
    delay_us(30);
    set_input();
    taskEXIT_CRITICAL(&s_dht_mux);

    /* ---- Wait for sensor response ---- */
    if (wait_level(0, 100) < 0) {
        ESP_LOGW(TAG, "No response (missing LOW ack)");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_level(1, 100) < 0) {
        ESP_LOGW(TAG, "No response (missing HIGH ack)");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_level(0, 100) < 0) {
        ESP_LOGW(TAG, "No response (missing start of data)");
        return ESP_ERR_TIMEOUT;
    }

    /* ---- Read 40 bits ---- */
    taskENTER_CRITICAL(&s_dht_mux);
    for (int i = 0; i < 40; i++) {
        int64_t start;
        uint32_t duration;

        start = esp_timer_get_time();
        while (gpio_get_level(s_gpio) == 0) {
            if ((esp_timer_get_time() - start) > 100) goto err_timeout;
        }

        start = esp_timer_get_time();
        while (gpio_get_level(s_gpio) == 1) {
            if ((esp_timer_get_time() - start) > 100) goto err_timeout;
        }
        duration = (uint32_t)(esp_timer_get_time() - start);

        raw[i / 8] <<= 1;
        if (duration > 40) {
            raw[i / 8] |= 1;
        }
    }
    taskEXIT_CRITICAL(&s_dht_mux);

    /* ---- Checksum ---- */
    uint8_t checksum = raw[0] + raw[1] + raw[2] + raw[3];
    if (checksum != raw[4]) {
        ESP_LOGW(TAG, "Checksum error: calc=%02x recv=%02x", checksum, raw[4]);
        return ESP_ERR_INVALID_CRC;
    }

    data->humidity    = (float)raw[0];
    data->temperature = (float)raw[2];

    ESP_LOGD(TAG, "T=%.1f°C  H=%.1f%%", data->temperature, data->humidity);
    return ESP_OK;

err_timeout:
    taskEXIT_CRITICAL(&s_dht_mux);
    ESP_LOGW(TAG, "Bit-bang timeout");
    return ESP_ERR_TIMEOUT;
}
