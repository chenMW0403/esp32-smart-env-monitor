#include "task_comm.h"
#include "esp_log.h"

static const char *TAG = "TASK_COMM";

QueueHandle_t g_sensor_queue  = NULL;
QueueHandle_t g_cmd_queue     = NULL;
SemaphoreHandle_t g_i2c_bus_mutex = NULL;
i2c_master_bus_handle_t g_i2c_bus = NULL;

void task_comm_init(void)
{
    /* Sensor data queue: holds up to SENSOR_QUEUE_DEPTH sensor_data_t structs */
    g_sensor_queue = xQueueCreate(SENSOR_QUEUE_DEPTH, sizeof(sensor_data_t));
    assert(g_sensor_queue != NULL);

    /* Remote command queue */
    g_cmd_queue = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(remote_cmd_t));
    assert(g_cmd_queue != NULL);

    /* I2C bus mutex (BH1750 and SSD1306 share the bus) */
    g_i2c_bus_mutex = xSemaphoreCreateMutex();
    assert(g_i2c_bus_mutex != NULL);

    ESP_LOGI(TAG, "Queues and mutexes created");
    ESP_LOGI(TAG, "  sensor_queue depth=%d", SENSOR_QUEUE_DEPTH);
    ESP_LOGI(TAG, "  cmd_queue depth=%d", CMD_QUEUE_DEPTH);
    ESP_LOGI(TAG, "  i2c_bus_mutex: ready");
}
