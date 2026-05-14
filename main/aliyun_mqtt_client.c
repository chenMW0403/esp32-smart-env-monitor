/**
 * @file aliyun_mqtt_client.c
 * @brief Alibaba Cloud IoT MQTT Client Implementation
 *
 * Authentication derivation for Alibaba Cloud IoT:
 *   clientId = {deviceName}
 *   securemode = 2 (TLS not required for TCP)
 *   signmethod = hmacsha256
 *
 *   content = "clientId{deviceName}deviceName{deviceName}productKey{productKey}ts{ts}"
 *   password = HMAC-SHA256(content, deviceSecret)
 *   username = "{deviceName}&{productKey}"
 *
 * After connection, subscribe to command topic and publish sensor data.
 */

#include "aliyun_mqtt_client.h"
#include "task_comm.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mbedtls/md.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "ALIYUN_MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

/* ---- HMAC-SHA256 helper ---- */
static esp_err_t hmac_sha256(const char *key, const char *data, size_t data_len,
                              unsigned char *output, size_t output_len)
{
    if (output_len < 32) return ESP_ERR_INVALID_SIZE;

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return mbedtls_md_hmac(md_info,
                           (const unsigned char *)key, strlen(key),
                           (const unsigned char *)data, data_len,
                           output) == 0 ? ESP_OK : ESP_FAIL;
}

/* ---- Build MQTT credentials ---- */
static void build_mqtt_uri(char *uri, size_t uri_len)
{
    snprintf(uri, uri_len, "mqtt://%s:%d", ALIYUN_MQTT_HOST, ALIYUN_MQTT_PORT);
}

static void build_client_id(char *buf, size_t len)
{
    /* clientId|securemode=2,signmethod=hmacsha256| */
    snprintf(buf, len, "%s|securemode=2,signmethod=hmacsha256|", ALIYUN_DEVICE_NAME);
}

static void build_username(char *buf, size_t len)
{
    snprintf(buf, len, "%s&%s", ALIYUN_DEVICE_NAME, ALIYUN_PRODUCT_KEY);
}

static esp_err_t build_password(char *buf, size_t len)
{
    /* Timestamp (fixed for simplicity; production should use real NTP) */
    const char *ts = "1234567890";

    /* content = clientId{clientId}deviceName{dn}productKey{pk}ts{ts} */
    char content[256];
    snprintf(content, sizeof(content),
             "clientId%sdeviceName%sproductKey%sts%s",
             ALIYUN_DEVICE_NAME, ALIYUN_DEVICE_NAME, ALIYUN_PRODUCT_KEY, ts);

    unsigned char hash[32];
    esp_err_t ret = hmac_sha256(ALIYUN_DEVICE_SECRET, content, strlen(content), hash, sizeof(hash));
    if (ret != ESP_OK) return ret;

    /* Convert to hex string */
    for (int i = 0; i < 32; i++) {
        snprintf(buf + i * 2, 3, "%02x", hash[i]);
    }
    buf[64] = '\0';
    return ESP_OK;
}

/* ---- MQTT Event Handler ---- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected to Alibaba Cloud IoT");
        s_connected = true;

        /* Subscribe to command topic */
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CMD, 1);
        ESP_LOGI(TAG, "Subscribed: %s", MQTT_TOPIC_CMD);

        /* Subscribe to property set topic */
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_SET, 1);
        ESP_LOGI(TAG, "Subscribed: %s", MQTT_TOPIC_SET);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        s_connected = false;
        break;

    case MQTT_EVENT_DATA: {
        /* Handle incoming command */
        char topic[128] = {0};
        char payload[512] = {0};
        int topic_len = event->topic_len < 127 ? event->topic_len : 127;
        int payload_len = event->data_len < 511 ? event->data_len : 511;
        strncpy(topic, event->topic, topic_len);
        strncpy(payload, event->data, payload_len);

        ESP_LOGI(TAG, "Received [%s]: %s", topic, payload);

        /* Parse command and push to cmd_queue */
        remote_cmd_t cmd = {CMD_NONE, 0};
        if (strstr(payload, "setInterval")) {
            cmd.type = CMD_SET_INTERVAL;
            /* Simple parse: look for "value":NNN */
            char *val = strstr(payload, "value");
            if (val) {
                val = strchr(val, ':');
                if (val) cmd.value = atoi(val + 1);
            }
        } else if (strstr(payload, "enterSleep")) {
            cmd.type = CMD_ENTER_SLEEP;
        } else if (strstr(payload, "reset")) {
            cmd.type = CMD_RESET;
        } else if (strstr(payload, "ledOn")) {
            cmd.type = CMD_LED_ON;
        } else if (strstr(payload, "ledOff")) {
            cmd.type = CMD_LED_OFF;
        }

        if (cmd.type != CMD_NONE) {
            xQueueSend(g_cmd_queue, &cmd, pdMS_TO_TICKS(100));
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error type: %d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

/* ---- Public API ---- */

esp_err_t aliyun_mqtt_init(void)
{
    char uri[128], client_id[128], username[128], password[128];

    build_mqtt_uri(uri, sizeof(uri));
    build_client_id(client_id, sizeof(client_id));
    build_username(username, sizeof(username));
    esp_err_t ret = build_password(password, sizeof(password));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build MQTT password");
        return ret;
    }

    ESP_LOGI(TAG, "MQTT URI: %s", uri);
    ESP_LOGI(TAG, "Client ID: %s", client_id);
    ESP_LOGI(TAG, "Username: %s", username);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri      = uri,
        .credentials.client_id   = client_id,
        .credentials.username    = username,
        .credentials.authentication.password = password,
        .network.reconnect_timeout_ms = 5000,
        .task.stack_size         = 8192,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT client start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Wait for connection (up to 10s) */
    for (int i = 0; i < 100 && !s_connected; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return s_connected ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t aliyun_mqtt_publish(const sensor_data_t *data)
{
    if (!s_connected || s_client == NULL) {
        ESP_LOGW(TAG, "MQTT not connected, skip publish");
        return ESP_ERR_INVALID_STATE;
    }

    /* Build JSON payload (Alibaba Cloud IoT format) */
    char json[512];
    snprintf(json, sizeof(json),
        "{"
        "\"id\":\"esp32_%08X\","
        "\"version\":\"1.0\","
        "\"method\":\"thing.event.property.post\","
        "\"params\":{"
            "\"temperature\":%.1f,"
            "\"humidity\":%.1f,"
            "\"light\":%.1f,"
            "\"airQuality\":%u,"
            "\"timestamp\":%u"
        "}"
        "}",
        (unsigned int)esp_random(),
        data->temperature, data->humidity,
        data->light_intensity, data->air_quality,
        data->timestamp
    );

    int msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_POST, json, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Publish failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published [%d]: %s", msg_id, json);
    return ESP_OK;
}

bool aliyun_mqtt_is_connected(void)
{
    return s_connected;
}
