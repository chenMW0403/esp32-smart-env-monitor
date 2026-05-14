#pragma once
/**
 * @file aliyun_mqtt_client.h
 * @brief Alibaba Cloud IoT MQTT Client
 *
 * Implements Alibaba Cloud IoT MQTT authentication:
 *   Client ID: {clientId}|securemode=2,signmethod=hmacsha256|
 *   Username:  {deviceName}&{productKey}
 *   Password:  HMAC-SHA256(deviceSecret, clientId{ts}...)
 *
 * Publishes sensor data and subscribes to command topic.
 */

#include "esp_err.h"
#include "smart_env_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and connect MQTT client to Alibaba Cloud IoT
 * @return ESP_OK on success
 */
esp_err_t aliyun_mqtt_init(void);

/**
 * @brief Publish sensor data as JSON to MQTT topic
 * @param data  Sensor data to publish
 * @return ESP_OK on success
 */
esp_err_t aliyun_mqtt_publish(const sensor_data_t *data);

/**
 * @brief Check if MQTT is connected
 */
bool aliyun_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif
