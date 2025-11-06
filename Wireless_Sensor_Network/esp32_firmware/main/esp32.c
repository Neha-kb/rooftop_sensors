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
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_sntp.h"
#include "time.h"
#include "esp_sleep.h"
#include "env_config.h"
#include "esp_mac.h"

// Wifi configurations included in env_config.h


// --- I2C (ADC) ---
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define MCP3426_ADDR                0x6E  // I2C address of MCP3426


// --- SPI (MAX31865) ---
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15


// --- MAX31865 TEMPERATURE ---
#define MAX31865_REG_CONFIG  0x00
#define MAX31865_REG_RTD_MSB 0x01
#define MAX31865_CONFIG_BIAS_ON   0x80
#define MAX31865_CONFIG_AUTO_CONV 0x40
#define MAX31865_CONFIG_4WIRE     0x00
static const float A = 3.9083e-3;
static const float B = -5.775e-7;


// --- GLOBALS ---
static const char *TAGMQTT = "ESP32_ADC_TEMP_MQTT";
static esp_mqtt_client_handle_t mqtt_client;
static char topic[64];
static spi_device_handle_t spi; // MAX31865 SPI handle
static SemaphoreHandle_t i2c_semaphore = NULL;


//  time variables for sending data
#define SUNRISE_HOUR 6
#define SUNSET_HOUR 18


typedef struct {
    float voltage;
    float current;
    float power;
    float temperature;
    time_t timestamp;
} sensor_sample_t;

static QueueHandle_t sensor_queue;



// --- HELPER: MAC ADDRESS ---
void get_mac_address(char *mac_str, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *TAG = "MCP3426";


// I²C initialization
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Start a one-shot 12-bit conversion
static esp_err_t mcp3426_start_conversion(uint8_t channel)
{
    uint8_t config = 0b10000000; // start, CH1, one-shot, 12-bit, gain=1
    if (channel == 2) config |= 0b00100000;  // select CH2
    return i2c_master_write_to_device(I2C_MASTER_NUM, MCP3426_ADDR,
                                      &config, 1, pdMS_TO_TICKS(50));
}

// Read 12-bit result
static esp_err_t mcp3426_read_12bit(int16_t *result)
{
    uint8_t data[3];
    while (1)
    {
        esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, MCP3426_ADDR,
                                                    data, sizeof(data), pdMS_TO_TICKS(50));
        if (err != ESP_OK) return err;

        uint8_t cfg = data[2];
        if ((cfg & 0x80) == 0) {  // RDY=0 → data ready
            int16_t raw = ((data[0] << 8) | data[1]);;
            // raw = raw >> 4;
            *result = raw;
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Wrapper to get ADC reading for a channel
int getADC(uint8_t channel)
{
    int16_t raw;
    if (mcp3426_start_conversion(channel) != ESP_OK) {
        ESP_LOGE(TAGMQTT, "Failed to start conversion on channel %d", channel);
        return 0;
    }
    if (mcp3426_read_12bit(&raw) != ESP_OK) {
        ESP_LOGE(TAGMQTT, "Failed to read ADC on channel %d", channel);
        return 0;
    }
    return raw;
}


float getVoltage()
{
    int adcReading = getADC(1);
    float voltage = ((float)adcReading / 2047.0f) * 2.048f; // ADC → volts
    voltage *= (118.0f + 4.02f) / 4.02f;                     // voltage divider
    return voltage;
}

float getCurrent()
{
    int adcReading = getADC(2);
    float voltage = ((float)adcReading / 2047.0f) * 2.048f; // volts
    float current = (voltage * 1.62f) - 0.33f; // apply sensor scaling factor
    current = (current/0.264f);
    return (current < 0.0f) ? 0.0f : current;
}


// --- MAX31865 INIT ---
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
    ESP_LOGI(TAGMQTT, "MAX31865 initialized");
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
        ESP_LOGE(TAGMQTT, "SPI transmit failed: %d", ret);
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
        ESP_LOGI(TAGMQTT, "MQTT connected");
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
    ESP_LOGI(TAGMQTT, "Connecting to WiFi %s...", WIFI_SSID);
    vTaskDelay(pdMS_TO_TICKS(5000));
}


void obtain_time(void) {
    ESP_LOGI(TAGMQTT, "Initializing SNTP...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();


    // Wait for time synchronization
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAGMQTT, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year >= (2016 - 1900)) {
        ESP_LOGI(TAGMQTT, "Time synchronized successfully.");
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAGMQTT, "Current local time: %s", strftime_buf);
    } 
    else {
        ESP_LOGW(TAGMQTT, "Failed to synchronize time.");
    }
}


bool is_daytime(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    return (hour >= SUNRISE_HOUR && hour < SUNSET_HOUR);
}



void sleep_until_sunrise(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hours_to_sunrise = (24 + SUNRISE_HOUR - timeinfo.tm_hour) % 24;
    int seconds_to_sunrise = hours_to_sunrise * 3600;

    ESP_LOGI(TAGMQTT, "Sleeping for %d hours until sunrise...", hours_to_sunrise);
    esp_sleep_enable_timer_wakeup((uint64_t)seconds_to_sunrise * 1000000ULL);
    esp_deep_sleep_start();
}


void sensor_task(void *pvParameters) {
    
    while (1) {
        float voltage = getVoltage();
        float current = getCurrent();
        float power = voltage * current;
        float temp = getTemperature();
        time_t timestamp = time(NULL);

        printf("Voltage: %.2f V, Current: %.2f A, Power: %.2f W, Temp: %.2f C, Timestamp: %s",
               voltage, current, power, temp, ctime(&timestamp));

        vTaskDelay(pdMS_TO_TICKS(2000)); // delay in millisecond
    }
}



void wifi_mqtt_task(void *pvParameters) {

    while (1) {
        // Read samples from sensor queue
        sensor_sample_t sample;
        while (xQueueReceive(sensor_queue, &sample, 0)) {
            char payload[200];
            snprintf(payload, sizeof(payload),
                "{\"voltage\": %.2f, \"current\": %.2f, \"power\": %.2f, \"temperature\": %.2f, \"timestamp\": %lld}",
                    sample.voltage, sample.current, sample.power, sample.temperature, (long long)sample.timestamp);

            int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
            ESP_LOGI(TAGMQTT, "Published: %s (msg_id=%d)", payload, msg_id);
        }

        // Check if it's nighttime
        if (!is_daytime()) {
            ESP_LOGI(TAGMQTT, "Nighttime detected. Entering deep sleep...");
            sleep_until_sunrise();
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // delay in millisecond
    }
}



void app_main(void) {

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();            // initialize WiFi
    obtain_time();              // synchronize time via SNTP
    // mqtt_app_start();     // start MQTT client

    // Initialize hardware
    i2c_master_init();          // initialize I2C bus
    max31865_init();            // initialize MAX31865

    // Create queue for sensor samples
    sensor_queue = xQueueCreate(20, sizeof(sensor_sample_t));

    // Setup MQTT topic
    char macID[13];
    get_mac_address(macID, sizeof(macID));
    snprintf(topic, sizeof(topic), "sensor/%s", macID);

    // Create FreeRTOS tasks pinned to cores
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 2, NULL, 0);     // Core 0
    xTaskCreatePinnedToCore(wifi_mqtt_task, "wifi_mqtt_task", 8192, NULL, 2, NULL, 1); // Core 1
}




// // --- MAIN ---
// void app_main(void) {
//     // Initialize NVS
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);


//     wifi_init_sta();            // initialize WiFi
//     obtain_time();              // synchronize time via SNTP

//     i2c_master_init();          // initialize I2C bus
//     max31865_init();            // initialize MAX31865


//     // Setup MQTT topic
//     char macID[13];
//     get_mac_address(macID, sizeof(macID));
//     snprintf(topic, sizeof(topic), "sensor/%s", macID);
//     // mqtt_app_start();



//     // Publish Loop
//     while (1) {

//         if (!is_daytime()) {
//         ESP_LOGI(TAGMQTT, "Nighttime detected. Entering deep sleep...");
//         sleep_until_sunrise();
//     }

//         float voltage = getVoltage();
//         float current = getCurrent();
//         float power = voltage * current;
//         float temperature = getTemperature();

//         char payload[200];
//         snprintf(payload, sizeof(payload),
//                  "{\"voltage\": %.2f, \"current\": %.2f, \"power\": %.2f, \"temperature\": %.2f}",
//                  voltage, current, power, temperature);
//         printf("%s\n", payload);

//         int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
//         ESP_LOGI(TAGMQTT, "Published to %s: %s (msg_id=%d)", topic, payload, msg_id);
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }