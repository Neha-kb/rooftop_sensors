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
#include "driver/i2c.h"

// --- Wi-Fi credentials ---
#define WIFI_SSID      "xxx"
#define WIFI_PASS      "xxx"

// --- MQTT broker ---
#define MQTT_URI       "mqtt://xxx.xxx.xx.xxx:1883"

// --- I2C Configuration ---
#define I2C_MASTER_SCL_IO           22      // SCL GPIO
#define I2C_MASTER_SDA_IO           21      // SDA GPIO
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000  // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define ADC_ADDR                    0x6E

static const char *TAG = "ESP32_ADC_MQTT";
static esp_mqtt_client_handle_t mqtt_client;
static char topic[64];

// --- Generate unique MAC ID ---
void get_mac_address(char *mac_str, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// --- I2C initialization ---
void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
}

// --- Helper: Read ADC value ---
int getADC(int channel) {
    uint8_t config;
    uint8_t data[2] = {0};

    if (channel == 1)
        config = 0b10000000;  // Channel 1
    else
        config = 0b10100000;  // Channel 2

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADC_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, config, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(10));

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADC_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    int raw_adc = ((data[0] << 8) | data[1]);
    ESP_LOGI(TAG, "Channel %d raw ADC: %d", channel, raw_adc);

    return raw_adc;
}

// --- Voltage & Current calculation ---
float getVoltage() {
    int adcReading = getADC(1);
    float voltage = ((float)adcReading / 2047.0f) * 2.048f;
    voltage *= (118.0f + 4.02f) / 4.02f;  // Scale using voltage divider
    return voltage;
}

float getCurrent() {
    int adcReading = getADC(2);
    float voltage_mV = ((float)adcReading / 40955.0f) * 2048.0f;
    float current = ((voltage_mV * 1.62f) / 1000.0f);
    return (current < 0.0f) ? 0.0f : current;
}

// --- MQTT event handler ---
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received: %.*s", event->data_len, event->data);
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
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// --- Wi-Fi initialization ---
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
    vTaskDelay(pdMS_TO_TICKS(5000));
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

    // Initialize I2C
    i2c_master_init();

    // Prepare MQTT topic
    char macID[13];
    get_mac_address(macID, sizeof(macID));
    snprintf(topic, sizeof(topic), "sensor/%s", macID);
    ESP_LOGI(TAG, "Device topic: %s", topic);

    // Start MQTT client
    mqtt_app_start();

    // --- Publish loop ---
    while (1) {
        float voltage = getVoltage();
        float current = getCurrent();
        float power = voltage * current;

        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"voltage\": %.2f, \"current\": %.2f, \"power\": %.2f}",
                 voltage, current, power);

        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published to %s: %s (msg_id=%d)", topic, payload, msg_id);

        vTaskDelay(pdMS_TO_TICKS(5000));  // Publish every 5 seconds
    }
}
