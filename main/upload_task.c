/**
 * @file upload_task.c
 * @brief Network Upload Task (MQTT to Alibaba Cloud IoT)
 *
 * Waits for sensor data on g_sensor_queue and publishes via MQTT.
 * Runs on Core 0 since Wi-Fi/TCP stack is bound there.
 */

#include "upload_task.h"
#include "task_comm.h"
#include "smart_env_config.h"
#include "aliyun_mqtt_client.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UPLOAD_TASK";

static void upload_task_func(void *pvParameters)
{
    sensor_data_t data;

    ESP_LOGI(TAG, "Upload task started on core %d", xPortGetCoreID());

    while (1) {
        /* Block until sensor data arrives */
        if (xQueueReceive(g_sensor_queue, &data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Received sensor data, uploading...");

            if (aliyun_mqtt_is_connected()) {
                esp_err_t ret = aliyun_mqtt_publish(&data);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Publish failed: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGW(TAG, "MQTT not connected, data discarded");
            }
        }
    }
}

esp_err_t upload_task_start(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(
        upload_task_func,
        "upload_task",
        TASK_STACK_UPLOAD,
        NULL,
        TASK_PRIO_UPLOAD,
        NULL,
        0  /* Core 0 (same as Wi-Fi stack) */
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create upload task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Upload task created, pinned to Core 0");
    return ESP_OK;
}
