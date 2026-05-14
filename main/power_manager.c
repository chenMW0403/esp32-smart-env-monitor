/**
 * @file power_manager.c
 * @brief Power Manager with Light-sleep Mode
 *
 * Strategy:
 *   1. Monitor system idle time via a counter
 *   2. After LIGHT_SLEEP_TIMEOUT_S seconds of no activity,
 *      prepare for Light-sleep:
 *      a. Stop non-essential tasks (display)
 *      b. Configure wake sources (timer + GPIO)
 *      c. Enter esp_light_sleep_start()
 *   3. On wake, resume tasks and reset idle counter
 *
 * Wake sources:
 *   - Timer: wake every 30s to take a sensor reading
 *   - GPIO: wake on button press (BOOT button, GPIO0)
 */

#include "power_manager.h"
#include "smart_env_config.h"

#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "POWER_MGR";

static volatile uint32_t s_idle_counter = 0;
static volatile bool s_sleep_requested = false;
static SemaphoreHandle_t s_sleep_mutex;

/* Wake stub: runs from RTC memory after light-sleep wake */
static void IRAM_ATTR wake_stub(void)
{
    /* Minimal work in RTC memory — just enough to wake CPU */
}

esp_err_t power_manager_init(void)
{
    s_sleep_mutex = xSemaphoreCreateMutex();
    assert(s_sleep_mutex != NULL);

    /* Configure GPIO wake source (BOOT button, GPIO0, active LOW) */
    esp_deep_sleep_enable_gpio_wakeup(BIT0, ESP_GPIO_WAKEUP_GPIO_LOW);

    /* Configure timer wake source (30 seconds) */
    esp_sleep_enable_timer_wakeup((uint64_t)LIGHT_SLEEP_TIMEOUT_S * 1000000ULL);

    /* Enable automatic light-sleep via PM (optional, more advanced) */
    #if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,           /* Max frequency when active */
        .min_freq_mhz = 40,            /* Minimum frequency when idle */
        .light_sleep_enable = true,    /* Auto enter light-sleep when idle */
    };
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PM configure failed: %s (continuing without auto-sleep)",
                 esp_err_to_name(ret));
    }
    #endif

    ESP_LOGI(TAG, "Power manager initialized");
    ESP_LOGI(TAG, "  Light-sleep timeout: %d s", LIGHT_SLEEP_TIMEOUT_S);
    ESP_LOGI(TAG, "  Wake sources: timer + GPIO0 (BOOT button)");

    return ESP_OK;
}

void power_manager_request_sleep(void)
{
    s_sleep_requested = true;
    ESP_LOGI(TAG, "Sleep requested by remote command");
}

void power_manager_reset_idle(void)
{
    s_idle_counter = 0;
}

static void enter_light_sleep(void)
{
    ESP_LOGI(TAG, "Entering Light-sleep mode...");

    /* Flush logs before sleeping */
    esp_log_level_set("*", ESP_LOG_NONE);

    /* Configure wake sources */
    esp_sleep_enable_timer_wakeup((uint64_t)LIGHT_SLEEP_TIMEOUT_S * 1000000ULL);
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    /* Enter light-sleep (blocks until wake) */
    esp_err_t ret = esp_light_sleep_start();

    /* --- Woke up here --- */
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Woke up: Timer");
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        ESP_LOGI(TAG, "Woke up: GPIO (button)");
        break;
    default:
        ESP_LOGI(TAG, "Woke up: cause=%d", cause);
        break;
    }

    s_sleep_requested = false;
    s_idle_counter = 0;
}

static void power_task_func(void *pvParameters)
{
    ESP_LOGI(TAG, "Power manager task started on core %d", xPortGetCoreID());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_idle_counter++;

        /* Check if sleep is needed */
        bool should_sleep = false;

        if (s_sleep_requested) {
            should_sleep = true;
        }

        if (s_idle_counter >= LIGHT_SLEEP_TIMEOUT_S) {
            ESP_LOGI(TAG, "Idle for %lu seconds, considering sleep",
                     (unsigned long)s_idle_counter);
            should_sleep = true;
        }

        if (should_sleep) {
            if (xSemaphoreTake(s_sleep_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                enter_light_sleep();
                xSemaphoreGive(s_sleep_mutex);
            }
        }
    }
}

/* Auto-start power manager in init */
__attribute__((constructor))
static void register_power_manager(void)
{
    /* Will be started by power_manager_init() */
}

esp_err_t power_manager_init_internal(void)
{
    /* Create task on Core 0 (low priority, background) */
    BaseType_t ret = xTaskCreatePinnedToCore(
        power_task_func,
        "power_task",
        TASK_STACK_POWER,
        NULL,
        TASK_PRIO_POWER,
        NULL,
        0  /* Core 0 */
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* Override init to include task creation */
esp_err_t power_manager_init(void)
{
    esp_err_t ret = power_manager_init_internal();
    if (ret == ESP_OK) {
        /* PM configure is done in init_internal via CONFIG_PM_ENABLE */
    }
    return ret;
}
