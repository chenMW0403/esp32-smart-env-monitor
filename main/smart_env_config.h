#pragma once

/* ============================================================
 *  smart_env_config.h
 *  Project-wide configuration: pins, credentials, intervals
 * ============================================================ */

#include <stdint.h>

/* ---------- Wi-Fi ---------- */
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#define WIFI_MAX_RETRY      10

/* ---------- Alibaba Cloud IoT ---------- */
/*
 * Replace with your device's "三元组" (triple):
 *   ProductKey / DeviceName / DeviceSecret
 * Obtain from Alibaba Cloud IoT console.
 */
#define ALIYUN_PRODUCT_KEY      "YOUR_PRODUCT_KEY"
#define ALIYUN_DEVICE_NAME      "YOUR_DEVICE_NAME"
#define ALIYUN_DEVICE_SECRET    "YOUR_DEVICE_SECRET"
#define ALIYUN_REGION           "cn-shanghai"

/* Derived MQTT endpoints (do NOT change) */
#define ALIYUN_MQTT_HOST        ALIYUN_PRODUCT_KEY ".iot-as-mqtt." ALIYUN_REGION ".aliyuncs.com"
#define ALIYUN_MQTT_PORT        1883

/* MQTT Topics (Alibaba Cloud format) */
#define MQTT_TOPIC_SYS_PREFIX   "/" ALIYUN_PRODUCT_KEY "/" ALIYUN_DEVICE_NAME
#define MQTT_TOPIC_POST         MQTT_TOPIC_SYS_PREFIX "/user/update"
#define MQTT_TOPIC_SET          MQTT_TOPIC_SYS_PREFIX "/user/get"
#define MQTT_TOPIC_CMD          MQTT_TOPIC_SYS_PREFIX "/user/data"

/* ---------- DHT11 (GPIO) ---------- */
#define DHT11_GPIO_PIN          GPIO_NUM_4

/* ---------- BH1750 (I2C) ---------- */
#define BH1750_I2C_SDA_PIN      GPIO_NUM_21
#define BH1750_I2C_SCL_PIN      GPIO_NUM_22
#define BH1750_I2C_PORT         I2C_NUM_0
#define BH1750_I2C_ADDR         0x23

/* ---------- MQ135 (ADC) ---------- */
#define MQ135_ADC_CHANNEL       ADC_CHANNEL_0   /* GPIO1 */
#define MQ135_ADC_UNIT          ADC_UNIT_1
#define MQ135_ADC_ATTEN         ADC_ATTEN_DB_12

/* ---------- SSD1306 OLED (I2C, shared bus) ---------- */
#define OLED_I2C_ADDR           0x3C
#define OLED_WIDTH              128
#define OLED_HEIGHT             64

/* ---------- Task Intervals (ms) ---------- */
#define SENSOR_READ_INTERVAL_MS     3000    /* 3 seconds */
#define UPLOAD_INTERVAL_MS          10000   /* 10 seconds */
#define DISPLAY_REFRESH_INTERVAL_MS 2000    /* 2 seconds */

/* ---------- Task Priorities ---------- */
#define TASK_PRIO_SENSOR        5
#define TASK_PRIO_UPLOAD        4
#define TASK_PRIO_DISPLAY       3
#define TASK_PRIO_POWER         2

/* ---------- Task Stack Sizes (bytes) ---------- */
#define TASK_STACK_SENSOR       4096
#define TASK_STACK_UPLOAD       8192
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_POWER        2048

/* ---------- Queue Depths ---------- */
#define SENSOR_QUEUE_DEPTH      5
#define CMD_QUEUE_DEPTH         3

/* ---------- Power Management ---------- */
#define LIGHT_SLEEP_TIMEOUT_S   30  /* Enter light-sleep after 30s idle */

/* ---------- Sensor Data Structure ---------- */
typedef struct {
    float temperature;      /* °C, from DHT11 */
    float humidity;         /* %RH, from DHT11 */
    float light_intensity;  /* lux, from BH1750 */
    uint16_t air_quality;   /* raw ADC, from MQ135 */
    uint32_t timestamp;     /* seconds since boot */
} sensor_data_t;

/* ---------- Remote Command Structure ---------- */
typedef enum {
    CMD_NONE = 0,
    CMD_SET_INTERVAL,
    CMD_ENTER_SLEEP,
    CMD_RESET,
    CMD_LED_ON,
    CMD_LED_OFF,
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    int32_t    value;
} remote_cmd_t;
