#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Configuration Definitions
#define ADC_CHANNEL ADC_CHANNEL_0
#define ADC_UNIT ADC_UNIT_1
#define LM35_MV_PER_DEG_C 10
#define LED_GPIO GPIO_NUM_2
#define WIFI_SSID "AndroidAP7681"
#define WIFI_PASSWORD "87654321"
#define COLLECTOR_IP "192.168.251.61"  //change to your IP
#define COLLECTOR_PORT 12345

// Task Priorities 
// (Lower number means higher priority)
#define DATA_ACQUISITION_PRIORITY (configMAX_PRIORITIES - 1)
#define NETWORK_AGENT_PRIORITY (configMAX_PRIORITIES - 2)
#define LED_PATTERN_PRIORITY (configMAX_PRIORITIES - 3)

// Queue Configuration
#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(TemperatureData)

// Logging Tag
static const char *TAG = "TemperatureMonitor";

// Global Handles
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
int udp_socket;
struct sockaddr_in collector_addr;

// Bi-directional Queues
QueueHandle_t temperatureDataQueue;
QueueHandle_t networkResponseQueue;

// Temperature Data Structure
typedef struct {
    float temperature;
    uint32_t timestamp;
    bool sendSuccess;
} TemperatureData;

// LED Pattern Context
typedef struct {
    uint8_t patternState;
    bool isInverted;
    TickType_t lastUpdateTime;
} LEDPatternContext;

// WiFi Event Handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, 
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// LED Pattern Generation Task
void led_pattern_task(void *pvParameters) {
    LEDPatternContext context = {
        .patternState = 0,
        .isInverted = false,
        .lastUpdateTime = 0
    };

    while (1) {
        TickType_t currentTime = xTaskGetTickCount();
        
        // Pattern update interval (250ms)
        if ((currentTime - context.lastUpdateTime) >= pdMS_TO_TICKS(250)) {
            context.lastUpdateTime = currentTime;

            // Breathing pattern
            context.patternState = (context.patternState + 1) % 16;
            context.isInverted = (context.patternState >= 8);
            
            uint8_t intensity = context.isInverted 
                ? 16 - (context.patternState - 8)
                : context.patternState;
            
            gpio_set_level(LED_GPIO, intensity > 8 ? 1 : 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Prevent task starvation
    }
}

// WiFi Initialization
void wifi_init_sta() {
    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                        &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                        &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

// ADC Initialization
void init_adc() {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle);
}

// LED Initialization
void init_led() {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

// Temperature Reading
float read_temperature() {
    int raw_adc = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_adc);

    int voltage = 0;
    adc_cali_raw_to_voltage(cali_handle, raw_adc, &voltage);

    float temperature = voltage / (float)LM35_MV_PER_DEG_C;

    return temperature;
}

// UDP Socket Initialization
void init_udp_socket() {
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }

    memset(&collector_addr, 0, sizeof(collector_addr));
    collector_addr.sin_family = AF_INET;
    collector_addr.sin_port = htons(COLLECTOR_PORT);
    inet_pton(AF_INET, COLLECTOR_IP, &collector_addr.sin_addr);

    ESP_LOGI(TAG, "UDP socket created");
}

// Data Acquisition Task
void data_acquisition_task(void *pvParameters) {
    TemperatureData tempData;
    
    while (1) {
        // Read temperature
        tempData.temperature = read_temperature();
        tempData.timestamp = xTaskGetTickCount();
        tempData.sendSuccess = false;

        // Send to network agent queue
        if (xQueueSend(temperatureDataQueue, &tempData, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Temperature queue full, data dropped");
        }

        // Optional: Check for network agent response
        TemperatureData responseData;
        if (xQueueReceive(networkResponseQueue, &responseData, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Network Response - Temperature: %.2f, Send Success: %s", 
                     responseData.temperature, responseData.sendSuccess ? "Yes" : "No");
        }

        // Local temperature print
        printf("Temperature: %.2f C\n", tempData.temperature);

        vTaskDelay(pdMS_TO_TICKS(4500)); // Maintain 5-second total cycle
    }
}

// Network Agent Task
void network_agent_task(void *pvParameters) {
    TemperatureData tempData;
    
    while (1) {
        // Wait for temperature data
        if (xQueueReceive(temperatureDataQueue, &tempData, portMAX_DELAY) == pdTRUE) {
            // Prepare and send message
            char message[64];
            snprintf(message, sizeof(message), "Temperature: %.2f C, Timestamp: %lu\n", 
                     tempData.temperature, tempData.timestamp);
            
            // Send temperature to collector
            int sendResult = sendto(udp_socket, message, strlen(message), 0, 
                                    (struct sockaddr *)&collector_addr, sizeof(collector_addr));
            
            // Update send success status
            tempData.sendSuccess = (sendResult >= 0);
            
            if (sendResult < 0) {
                ESP_LOGE(TAG, "Failed to send temperature data");
            }

            // Send response back to data acquisition task
            xQueueSend(networkResponseQueue, &tempData, pdMS_TO_TICKS(100));
        }
    }
}

// Main Application Entry Point
void app_main() {
    // Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Peripherals and Network
    wifi_init_sta();
    init_adc();
    init_led();
    init_udp_socket();

    // Create Queues
    temperatureDataQueue = xQueueCreate(QUEUE_LENGTH, sizeof(TemperatureData));
    networkResponseQueue = xQueueCreate(QUEUE_LENGTH, sizeof(TemperatureData));

    if (temperatureDataQueue == NULL || networkResponseQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    // Create Tasks with Different Priorities
    xTaskCreate(data_acquisition_task, "DataAcquisitionTask", 4096, 
                NULL, DATA_ACQUISITION_PRIORITY, NULL);
    xTaskCreate(network_agent_task, "NetworkAgentTask", 4096, 
                NULL, NETWORK_AGENT_PRIORITY, NULL);
    xTaskCreate(led_pattern_task, "LEDPatternTask", 4096, 
                NULL, LED_PATTERN_PRIORITY, NULL);
}