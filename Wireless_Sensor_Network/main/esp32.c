#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"

// --- Wi-Fi credentials ---

#define WIFI_SSID      "EB2D"
#define WIFI_PASS      "95454641"

// --- MQTT broker ---
#define MQTT_URI       "mqtt://193.196.52.201:1883"

static const char *TAG = "MQTT_EXAMPLE";
static esp_mqtt_client_handle_t mqtt_client;
static char topic[64];

// --- Generate unique MAC ID ---
void get_mac_address(char *mac_str, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// --- MQTT event handler ---
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI("MQTT", "Disconnected");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI("MQTT", "Received data: %.*s", event->data_len, event->data);
            break;
        default:
            break;
    }
}


// --- Start MQTT client ---
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client!");
    return;
}

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);


}

// --- Initialize Wi-Fi ---
void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Connecting to WiFi %s...", WIFI_SSID);
    vTaskDelay(pdMS_TO_TICKS(5000)); // wait for connection
}

// --- Main application ---
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    wifi_init_sta();

    // Prepare MQTT topic
    char macID[13];
    get_mac_address(macID, sizeof(macID));
    snprintf(topic, sizeof(topic), "sensor/%s", macID);
    ESP_LOGI(TAG, "Device topic: %s", topic);

    // Start MQTT client
    mqtt_app_start();

    // --- Publish loop ---
    while (1) {
        int temperature = 20 + (esp_random() % 10); // 20-29
        int humidity = 40 + (esp_random() % 30);    // 40-69

        char payload[128];
        snprintf(payload, sizeof(payload), "{\"temperature\": %d, \"humidity\": %d}", temperature, humidity);

        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published to %s: %s (msg_id=%d)", topic, payload, msg_id);

        vTaskDelay(pdMS_TO_TICKS(5000)); // Publish every 5 seconds
    }
}
