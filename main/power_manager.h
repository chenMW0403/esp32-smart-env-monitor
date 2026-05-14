#pragma once
/**
 * @file power_manager.h
 * @brief Low-Power Management (Light-sleep Mode)
 *
 * Monitors system activity. After a configurable idle period,
 * enters ESP32 Light-sleep mode which:
 *   - Suspends CPU cores
 *   - Keeps RTC memory and peripherals alive
 *   - Can wake on GPIO (button), timer, or UART
 *   - Reduces current from ~80mA to ~0.8mA
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize power management and start monitoring task
 */
esp_err_t power_manager_init(void);

/**
 * @brief Manually request sleep (e.g., from remote command)
 */
void power_manager_request_sleep(void);

/**
 * @brief Reset idle timer (call from other tasks when active)
 */
void power_manager_reset_idle(void);

#ifdef __cplusplus
}
#endif
