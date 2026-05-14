/**
 * @file display_task.c
 * @brief OLED Display Task
 *
 * Reads latest sensor data from queue and renders on SSD1306 OLED.
 * I2C access is mutex-protected since BH1750 shares the bus.
 * Uses the global I2C bus handle (g_i2c_bus) from task_comm.
 */

#include "display_task.h"
#include "task_comm.h"
#include "smart_env_config.h"

#include "ssd1306.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "DISPLAY_TASK";

static ssd1306_handle_t s_oled;

static esp_err_t init_display(void)
{
    /* Initialize OLED on the shared I2C bus (mutex protected) */
    if (xSemaphoreTake(g_i2c_bus_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        esp_err_t ret = ssd1306_init(&s_oled, g_i2c_bus, OLED_I2C_ADDR);
        xSemaphoreGive(g_i2c_bus_mutex);
        return ret;
    }
    ESP_LOGE(TAG, "I2C mutex timeout");
    return ESP_ERR_TIMEOUT;
}

static void render_data(const sensor_data_t *data)
{
    char line[32];

    ssd1306_clear(&s_oled);

    /* Title */
    ssd1306_draw_string(&s_oled, 0, 0, "== ENV MONITOR ==");

    /* Temperature & Humidity (from DHT11) */
    snprintf(line, sizeof(line), "T: %.1f C", data->temperature);
    ssd1306_draw_string(&s_oled, 0, 2, line);

    snprintf(line, sizeof(line), "H: %.1f %%", data->humidity);
    ssd1306_draw_string(&s_oled, 0, 3, line);

    /* Light intensity (from BH1750) */
    snprintf(line, sizeof(line), "Lux: %.0f", data->light_intensity);
    ssd1306_draw_string(&s_oled, 0, 4, line);

    /* Air quality (from MQ135) */
    snprintf(line, sizeof(line), "AQI: %u%%", data->air_quality);
    ssd1306_draw_string(&s_oled, 0, 5, line);

    /* Uptime */
    snprintf(line, sizeof(line), "Up: %us", data->timestamp);
    ssd1306_draw_string(&s_oled, 0, 7, line);
}

static void display_task_func(void *pvParameters)
{
    sensor_data_t latest_data = {0};
    sensor_data_t new_data;

    ESP_LOGI(TAG, "Display task started on core %d", xPortGetCoreID());

    /* Show startup message */
    if (xSemaphoreTake(g_i2c_bus_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ssd1306_clear(&s_oled);
        ssd1306_draw_string(&s_oled, 10, 2, "ESP32-S3");
        ssd1306_draw_string(&s_oled, 10, 3, "Smart Env");
        ssd1306_draw_string(&s_oled, 10, 4, "Monitor v1");
        ssd1306_update(&s_oled);
        xSemaphoreGive(g_i2c_bus_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));  /* Show splash for 2s */

    while (1) {
        /* Drain queue to get latest data (non-blocking) */
        while (xQueueReceive(g_sensor_queue, &new_data, 0) == pdTRUE) {
            latest_data = new_data;
        }

        /* Render on OLED (mutex protected) */
        if (xSemaphoreTake(g_i2c_bus_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            render_data(&latest_data);
            ssd1306_update(&s_oled);
            xSemaphoreGive(g_i2c_bus_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_INTERVAL_MS));
    }
}

esp_err_t display_task_start(void)
{
    esp_err_t ret = init_display();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed");
        return ret;
    }

    BaseType_t xret = xTaskCreatePinnedToCore(
        display_task_func,
        "display_task",
        TASK_STACK_DISPLAY,
        NULL,
        TASK_PRIO_DISPLAY,
        NULL,
        1  /* Core 1 (sensor also on Core 1) */
    );

    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Display task created, pinned to Core 1");
    return ESP_OK;
}
