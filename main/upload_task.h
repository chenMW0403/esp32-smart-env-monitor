#pragma once
/**
 * @file upload_task.h
 * @brief Network Upload Task
 *
 * Reads sensor data from queue and publishes to Alibaba Cloud IoT
 * via MQTT. Runs on Core 0 (Wi-Fi and TCP/IP stack runs here).
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t upload_task_start(void);

#ifdef __cplusplus
}
#endif
