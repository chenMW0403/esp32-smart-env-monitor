/**
 * @file main.c
 * @brief Smart Environment Monitor — Application Entry Point
 *
 * System architecture:
 *
 *   ┌─────────────┐     sensor_queue      ┌──────────────┐
 *   │ sensor_task  │ ────────────────────> │ upload_task   │
 *   │ (Core 1)     │                       │ (Core 0)      │
 *   │  - DHT11     │                       │  - MQTT pub   │
 *   │  - BH1750    │     cmd_queue         │  - Alibaba    │
 *   │  - MQ135     │ <──────────────────── │    Cloud IoT  │
 *   └──────────────┘                       └──────────────┘
 *         │
 *         │ sensor_queue (shared with display)
 *         ▼
 *   ┌──────────────┐
 *   │ display_task  │
 *   │ (Core 1)      │
 *   │  - SSD1306    │
 *   │    OLED 128x64│
 *   └──────────────┘
 *
 *   ┌──────────────┐
 *   │ power_task    │   Monitors idle time
 *   │ (Core 0)      │   -> Light-sleep after 30s idle
 *   └──────────────┘
 *
 * Mutexes:
 *   g_i2c_bus_mutex: protects I2C bus (BH1750 + SSD1306)
 *
 * Boot sequence:
 *   1. NVS flash init
 *   2. Create queues & mutexes (task_comm)
 *   3. Wi-Fi connect
 *   4. MQTT connect (Alibaba Cloud IoT)
 *   5. Start sensor task (Core 1)
 *   6. Start upload task (Core 0)
 *   7. Start display task (Core 1)
 *   8. Start power manager (Core 0)
 */

#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "smart_env_config.h"
#include "task_comm.h"
#include "wifi_manager.h"
#include "aliyun_mqtt_client.h"
#include "sensor_task.h"
#include "upload_task.h"
#include "display_task.h"
#include "power_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Smart Environment Monitor v1.0");
    ESP_LOGI(TAG, "  ESP32-S3 + ESP-IDF + FreeRTOS");
    ESP_LOGI(TAG, "========================================");

    /* ---- Step 1: Initialize NVS ---- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS: erasing and re-initializing");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "[1/8] NVS initialized");

    /* ---- Step 2: Create communication primitives ---- */
    task_comm_init();
    ESP_LOGI(TAG, "[2/8] Queues and mutexes created");

    /* ---- Step 3: Connect to Wi-Fi ---- */
    ESP_LOGI(TAG, "[3/8] Connecting to Wi-Fi...");
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed! Continuing in offline mode.");
        /* Don't halt — sensor collection and display still work offline */
    } else {
        ESP_LOGI(TAG, "[3/8] Wi-Fi connected");
    }

    /* ---- Step 4: Connect to Alibaba Cloud IoT ---- */
    ESP_LOGI(TAG, "[4/8] Connecting to Alibaba Cloud IoT...");
    ret = aliyun_mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT connection failed! Data will not be uploaded.");
        /* Continue — display and sensor tasks still run */
    } else {
        ESP_LOGI(TAG, "[4/8] MQTT connected to Alibaba Cloud IoT");
    }

    /* ---- Step 5: Start sensor task ---- */
    ESP_LOGI(TAG, "[5/8] Starting sensor task...");
    ret = sensor_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor task failed to start!");
        /* Fatal — no point continuing without sensors */
    } else {
        ESP_LOGI(TAG, "[5/8] Sensor task running");
    }

    /* ---- Step 6: Start upload task ---- */
    ESP_LOGI(TAG, "[6/8] Starting upload task...");
    ret = upload_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Upload task failed to start!");
    } else {
        ESP_LOGI(TAG, "[6/8] Upload task running");
    }

    /* ---- Step 7: Start display task ---- */
    ESP_LOGI(TAG, "[7/8] Starting display task...");
    ret = display_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display task failed to start!");
    } else {
        ESP_LOGI(TAG, "[7/8] Display task running");
    }

    /* ---- Step 8: Start power manager ---- */
    ESP_LOGI(TAG, "[8/8] Starting power manager...");
    ret = power_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power manager failed to start!");
    } else {
        ESP_LOGI(TAG, "[8/8] Power manager running");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System fully operational!");
    ESP_LOGI(TAG, "  Sensors: DHT11 + BH1750 + MQ135");
    ESP_LOGI(TAG, "  Display: SSD1306 128x64 OLED");
    ESP_LOGI(TAG, "  Cloud: Alibaba Cloud IoT MQTT");
    ESP_LOGI(TAG, "  Power: Light-sleep after %ds idle", LIGHT_SLEEP_TIMEOUT_S);
    ESP_LOGI(TAG, "========================================");

    /* Main task returns — all work done by FreeRTOS tasks */
    /* The scheduler keeps running even after app_main() returns */
}
