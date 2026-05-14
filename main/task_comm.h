#pragma once
/**
 * @file task_comm.h
 * @brief Inter-task Communication: Queues & Mutexes
 *
 * Central definitions for FreeRTOS communication primitives.
 * All tasks include this header to access shared queues and mutexes.
 *
 * Communication architecture:
 *   sensor_task --[sensor_queue]--> upload_task  (data to cloud)
 *   sensor_task --[sensor_queue]--> display_task (data to screen)
 *   upload_task --[cmd_queue]-----> sensor_task  (remote commands)
 *
 * Shared resources protected by mutex:
 *   g_i2c_bus_mutex: protects I2C bus (BH1750 + SSD1306 share it)
 *   g_i2c_bus: shared I2C master bus handle
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"
#include "smart_env_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Queues ---- */
extern QueueHandle_t g_sensor_queue;    /* sensor_data_t: sensor -> upload/display */
extern QueueHandle_t g_cmd_queue;       /* remote_cmd_t: upload -> sensor */

/* ---- Mutexes ---- */
extern SemaphoreHandle_t g_i2c_bus_mutex;   /* Protects shared I2C bus */

/* ---- Shared I2C bus ---- */
extern i2c_master_bus_handle_t g_i2c_bus;   /* Shared I2C bus (BH1750 + SSD1306) */

/**
 * @brief Create all queues and mutexes. Call once from app_main().
 */
void task_comm_init(void);

#ifdef __cplusplus
}
#endif
