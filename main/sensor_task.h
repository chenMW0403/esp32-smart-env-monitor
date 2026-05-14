#pragma once
/**
 * @file sensor_task.h
 * @brief Sensor Data Collection Task
 *
 * Runs on Core 1 (high real-time requirement).
 * Reads DHT11, BH1750, and MQ135 sensors periodically.
 * Sends data to sensor_queue for upload and display tasks.
 * Handles incoming remote commands from cmd_queue.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and start the sensor collection task
 * @return ESP_OK on success
 */
esp_err_t sensor_task_start(void);

#ifdef __cplusplus
}
#endif
