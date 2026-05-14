#pragma once
/**
 * @file wifi_manager.h
 * @brief Wi-Fi Station Mode Manager
 *
 * Handles connection to AP with automatic retry.
 * Uses event group for synchronization: caller can wait for connection.
 */

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event bits exposed for other modules that want to wait */
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

extern EventGroupHandle_t g_wifi_event_group;

/**
 * @brief Initialize Wi-Fi in station mode and connect
 * @return ESP_OK if connected, ESP_FAIL on timeout
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Check if Wi-Fi is currently connected
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
