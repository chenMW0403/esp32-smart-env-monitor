idf_component_register(
    SRCS "main.c"
         "wifi_manager.c"
         "aliyun_mqtt_client.c"
         "sensor_task.c"
         "upload_task.c"
         "display_task.c"
         "power_manager.c"
         "task_comm.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event esp_netif mqtt json
             nvs_flash esp_pm esp_sleep
             dht11 bh1750 mq135 ssd1306
)
