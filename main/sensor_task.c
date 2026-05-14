/**
 * @file sensor_task.c
 * @brief Sensor Data Collection Task
 *
 * Core responsibilities:
 *   1. Periodically read all three sensors
 *   2. Pack data into sensor_data_t and send to queue
 *   3. Check cmd_queue for remote commands
 *   4. Protect I2C access with mutex (BH1750 on shared bus)
 *
 * Runs on Core 1 via xTaskCreatePinnedToCore for deterministic timing.
 */

#include "sensor_task.h"
#include "task_comm.h"
#include "smart_env_config.h"

#include "dht11.h"
#include "bh1750.h"
#include "mq135.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

static const char *TAG = "SENSOR_TASK";

/* Global sensor handles */
static bh1750_handle_t s_bh1750;
static mq135_handle_t  s_mq135;

/* Current sensor read interval (can be changed by remote command) */
static uint32_t s_read_interval_ms = SENSOR_READ_INTERVAL_MS;

static esp_err_t init_i2c_bus(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = BH1750_I2C_PORT,
        .sda_io_num = BH1750_I2C_SDA_PIN,
        .scl_io_num = BH1750_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bus_cfg, &g_i2c_bus);
}

static esp_err_t read_all_sensors(sensor_data_t *data)
{
    esp_err_t ret;

    /* --- DHT11 (GPIO, no mutex needed) --- */
    dht11_data_t dht;
    ret = dht11_read(&dht);
    if (ret == ESP_OK) {
        data->temperature = dht.temperature;
        data->humidity    = dht.humidity;
    } else {
        ESP_LOGW(TAG, "DHT11 read failed: %s (using last values)", esp_err_to_name(ret));
    }

    /* --- BH1750 (I2C, mutex protected) --- */
    if (xSemaphoreTake(g_i2c_bus_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        float lux = 0;
        ret = bh1750_read_lux(&s_bh1750, &lux);
        if (ret == ESP_OK) {
            data->light_intensity = lux;
        }
        xSemaphoreGive(g_i2c_bus_mutex);
    } else {
        ESP_LOGW(TAG, "I2C mutex timeout for BH1750");
    }

    /* --- MQ135 (ADC, no mutex needed) --- */
    uint16_t aqi = 0;
    ret = mq135_read_aqi(&s_mq135, &aqi);
    if (ret == ESP_OK) {
        data->air_quality = aqi;
    }

    data->timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);

    return ESP_OK;
}

static void handle_remote_commands(void)
{
    remote_cmd_t cmd;
    while (xQueueReceive(g_cmd_queue, &cmd, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Remote command: type=%d value=%ld", cmd.type, (long)cmd.value);
        switch (cmd.type) {
        case CMD_SET_INTERVAL:
            if (cmd.value >= 1000 && cmd.value <= 60000) {
                s_read_interval_ms = (uint32_t)cmd.value;
                ESP_LOGI(TAG, "Sensor interval set to %lu ms", (unsigned long)s_read_interval_ms);
            }
            break;
        case CMD_LED_ON:
            ESP_LOGI(TAG, "Command: LED ON");
            /* Can be extended to control actual LED */
            break;
        case CMD_LED_OFF:
            ESP_LOGI(TAG, "Command: LED OFF");
            break;
        case CMD_RESET:
            ESP_LOGW(TAG, "Command: RESET in 1s...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
            break;
        case CMD_ENTER_SLEEP:
            ESP_LOGI(TAG, "Command: Enter sleep mode");
            /* Will be handled by power_manager */
            break;
        default:
            break;
        }
    }
}

static void sensor_task_func(void *pvParameters)
{
    sensor_data_t data = {0};
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Sensor task started on core %d", xPortGetCoreID());

    while (1) {
        /* Handle any incoming remote commands */
        handle_remote_commands();

        /* Read all sensors */
        if (read_all_sensors(&data) == ESP_OK) {
            ESP_LOGI(TAG, "T=%.1f°C  H=%.1f%%  Lux=%.1f  AQI=%u",
                     data.temperature, data.humidity,
                     data.light_intensity, data.air_quality);

            /* Send to queue (non-blocking, drop oldest if full) */
            if (xQueueSend(g_sensor_queue, &data, 0) != pdTRUE) {
                /* Queue full, overwrite by receiving and discarding */
                sensor_data_t discard;
                xQueueReceive(g_sensor_queue, &discard, 0);
                xQueueSend(g_sensor_queue, &data, 0);
                ESP_LOGW(TAG, "Sensor queue was full, dropped oldest sample");
            }
        }

        /* Precise periodic delay */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(s_read_interval_ms));
    }
}

esp_err_t sensor_task_start(void)
{
    /* Initialize I2C bus */
    esp_err_t ret = init_i2c_bus();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize DHT11 (GPIO, no I2C) */
    ret = dht11_init(DHT11_GPIO_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DHT11 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize BH1750 (I2C, mutex protected) */
    if (xSemaphoreTake(g_i2c_bus_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ret = bh1750_init(&s_bh1750, g_i2c_bus, BH1750_I2C_ADDR);
        xSemaphoreGive(g_i2c_bus_mutex);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BH1750 init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        ESP_LOGE(TAG, "I2C mutex timeout during BH1750 init");
        return ESP_ERR_TIMEOUT;
    }

    /* Initialize MQ135 (ADC) */
    ret = mq135_init(&s_mq135, MQ135_ADC_UNIT, MQ135_ADC_CHANNEL, MQ135_ADC_ATTEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQ135 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create task pinned to Core 1 (high real-time requirement) */
    BaseType_t xret = xTaskCreatePinnedToCore(
        sensor_task_func,
        "sensor_task",
        TASK_STACK_SENSOR,
        NULL,
        TASK_PRIO_SENSOR,
        NULL,
        1  /* Core 1 */
    );

    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sensor task created, pinned to Core 1");
    return ESP_OK;
}
