#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"

#define LED_GPIO 2  // Most ESP32 boards have built-in LED on GPIO 2
#define TAG "TEMPERATURE_SIMULATOR"

void app_main(void) {
    // Configure LED GPIO
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // Seed the random number generator
    srand(esp_random());

    while (1) {
        // Generate random temperature between 0 and 50
        float temperature = (float)(rand() % 51);

        // Log the generated temperature
        ESP_LOGI(TAG, "Simulated Temperature: %.2f°C", temperature);

        // Check for extreme temperatures
        if (temperature > 40 || temperature < 10) {
            // Flash LED 3 times quickly
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED_GPIO, 1);  // LED on
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_GPIO, 0);  // LED off
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        // Wait for 5 seconds before next reading
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}