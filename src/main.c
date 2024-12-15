#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

#define ADC_CHANNEL ADC_CHANNEL_0 // GPIO36 (A0) corresponds to ADC_CHANNEL_0
#define ADC_UNIT ADC_UNIT_1 // Using ADC Unit 1
#define LM35_MV_PER_DEG_C 10 // LM35 outputs 10mV per degree Celsius
#define LED_GPIO GPIO_NUM_2 // Built-in LED on most ESP32 boards
#define WIFI_SSID "AndroidAP7681" // Replace with your Wi-Fi SSID
#define WIFI_PASSWORD "87654321" // Replace with your Wi-Fi password
#define COLLECTOR_IP "192.168.155.61"
#define COLLECTOR_PORT 12345

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static const char *TAG = "WiFi_Connect";

int udp_socket;
struct sockaddr_in collector_addr;

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

void init_led() {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

float read_temperature() {
    int raw_adc = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_adc);

    int voltage = 0;
    adc_cali_raw_to_voltage(cali_handle, raw_adc, &voltage);

    float temperature = voltage / (float)LM35_MV_PER_DEG_C;

    return temperature;
}

void flash_led() {
    gpio_set_level(LED_GPIO, 1); // Turn LED on
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    gpio_set_level(LED_GPIO, 0); // Turn LED off
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
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

void wifi_init_sta() {
    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

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

void send_temperature(float temperature) {
    char message[64];
    snprintf(message, sizeof(message), "Temperature: %.2f C\n", temperature);
    
    sendto(udp_socket, message, strlen(message), 0, (struct sockaddr *)&collector_addr, sizeof(collector_addr));
}
void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    init_adc();
    init_led();
    init_udp_socket();

    while (1) {
        float temperature = read_temperature();

        // Print temperature locally
        printf("Temperature: %.2f C\n", temperature);

        // Send temperature to collector
        send_temperature(temperature);

        // Flash the LED
        flash_led();

        // Delay for 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
