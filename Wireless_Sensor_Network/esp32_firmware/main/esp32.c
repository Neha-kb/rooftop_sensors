#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"


// --- CONFIGURATION ---
#define WIFI_SSID      "xxx"
#define WIFI_PASS      "xxx"
#define MQTT_URI       "mqtt://xxx.xxx.xx.xxx:1883"

// --- I2C (ADC) ---
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define ADC_ADDR                    0x6E

// --- SPI (MAX31865) ---
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15


// --- GLOBALS ---
static const char *TAG = "ESP32_ADC_TEMP_MQTT";
static esp_mqtt_client_handle_t mqtt_client;
static char topic[64];
static spi_device_handle_t spi; // MAX31865 SPI handle

// --- HELPER: MAC ADDRESS ---
void get_mac_address(char *mac_str, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// --- I2C INIT ---
void i2c_master_init(void) {
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

// --- ADC FUNCTIONS ---
int getADC(int channel) {
    uint8_t config;
    uint8_t data[2] = {0};

    config = (channel == 1) ? 0b10000000 : 0b10100000;

    // Write channel select
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADC_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, config, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(10));

    // Read result
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADC_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    int raw_adc = ((data[0] << 8) | data[1]);
    return raw_adc;
}

float getVoltage() {
    int adcReading = getADC(1);
    float voltage = ((float)adcReading / 2047.0f) * 2.048f;
    voltage *= (118.0f + 4.02f) / 4.02f;
    return voltage;
}

float getCurrent() {
    int adcReading = getADC(2);
    float voltage_mV = ((float)adcReading / 40955.0f) * 2048.0f;
    float current = ((voltage_mV * 1.62f) / 1000.0f);
    return (current < 0.0f) ? 0.0f : current;
}


// --- MAX31865 TEMPERATURE ---

#define MAX31865_REG_CONFIG  0x00
#define MAX31865_REG_RTD_MSB 0x01

#define MAX31865_CONFIG_BIAS_ON   0x80
#define MAX31865_CONFIG_AUTO_CONV 0x40
#define MAX31865_CONFIG_4WIRE     0x00

static const float A = 3.9083e-3;
static const float B = -5.775e-7;

void max31865_init(void) {
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 1,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 0));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));

    uint8_t tx_data[2] = { (MAX31865_REG_CONFIG | 0x80),
                           MAX31865_CONFIG_BIAS_ON | MAX31865_CONFIG_AUTO_CONV | MAX31865_CONFIG_4WIRE };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data };
    ret = spi_device_polling_transmit(spi, &t);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "MAX31865 initialized");
}

uint16_t max31865_read_rtd(void) {
    uint8_t tx_data[3] = { MAX31865_REG_RTD_MSB, 0x00, 0x00 };
    uint8_t rx_data[3] = {0};

    spi_transaction_t t = {0};
    t.length = 24; // bits
    t.tx_buffer = tx_data;
    t.rx_buffer = rx_data;
    t.flags = 0;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %d", ret);
        return 0;
    }

    uint16_t rtd = ((rx_data[1] << 8) | rx_data[2]) >> 1;
    return rtd;
}


float getTemperature() {
    uint16_t rtd = max31865_read_rtd();
    const float Rref = 430.0f;
    const float R0 = 100.0f;
    float Rt = ((float)rtd * Rref) / 32768.0f;
    float temp = (-A + sqrtf(A*A - 4*B*(1 - Rt/R0))) / (2*B);
    return temp;
}


// --- MQTT HANDLING ---
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    if (event->event_id == MQTT_EVENT_CONNECTED)
        ESP_LOGI(TAG, "MQTT connected");
}

static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


// --- WIFI INIT ---
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

// --- MAIN ---
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // wifi_init_sta();
    i2c_master_init();
    max31865_init();

    // Setup MQTT topic
    char macID[13];
    get_mac_address(macID, sizeof(macID));
    snprintf(topic, sizeof(topic), "sensor/%s", macID);
    // mqtt_app_start();

    // Publish Loop
    while (1) {
        float voltage = getVoltage();
        float current = getCurrent();
        float power = voltage * current;
        float temperature = getTemperature();

        char payload[200];
        snprintf(payload, sizeof(payload),
                 "{\"voltage\": %.2f, \"current\": %.2f, \"power\": %.2f, \"temperature\": %.2f}",
                 voltage, current, power, temperature);

        printf("%s\n", payload);


        // int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
        // ESP_LOGI(TAG, "Published to %s: %s (msg_id=%d)", topic, payload, msg_id);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
