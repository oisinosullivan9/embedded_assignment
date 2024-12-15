// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include <esp_system.h>
// #include <driver/gpio.h>
// #include <driver/uart.h>
// #include <esp_random.h>

// // Custom configuration parameters
// #define LED_GPIO        2  // Built-in LED pin for ESP32
// #define UART_NUM        UART_NUM_0
// #define UART_TX_PIN     1
// #define UART_RX_PIN     3
// #define UART_BAUD_RATE  115200

// // Task priorities
// #define METRIC_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )
// #define SERIAL_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3 )
// #define CONTROL_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )

// // Metric packet structure (byte-aligned)
// typedef struct __attribute__((packed)) {
//     uint32_t timestamp;
//     uint32_t free_heap;
//     float    cpu_temp;
//     uint8_t  cpu_usage;
//     uint8_t  task_count;
//     uint8_t  temp_status;  // 0: Normal, 1: Low Temp, 2: High Temp
// } SystemMetrics;

// // Communication packet
// typedef struct __attribute__((packed)) {
//     uint8_t  packet_type;
//     uint16_t payload_length;
//     SystemMetrics metrics;
//     uint16_t crc;
// } CommunicationPacket;

// // Global variables
// QueueHandle_t metrics_queue;
// QueueHandle_t control_queue;

// // Calculate CRC-16 for packet validation
// uint16_t calculate_crc16(uint8_t* data, size_t length) {
//     uint16_t crc = 0xFFFF;
//     for (size_t i = 0; i < length; i++) {
//         crc ^= data[i];
//         for (int j = 0; j < 8; j++) {
//             if (crc & 0x0001) {
//                 crc = (crc >> 1) ^ 0xA001;
//             } else {
//                 crc >>= 1;
//             }
//         }
//     }
//     return crc;
// }

// // Simulate temperature generation
// float generate_simulated_temperature() {
//     // Use ESP's random number generator to create temperature between 0 and 50
//     uint32_t random_value = esp_random();
//     return (random_value % 5001) / 100.0f;  // Scales to 0-50 range
// }

// // UART Initialization
// void uart_init() {
//     // Configure UART parameters
//     uart_config_t uart_config = {
//         .baud_rate = UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
//     };
    
//     // Install UART driver
//     ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
//     ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, 
//                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
//     ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 256, 256, 0, NULL, 0));
// }

// // Metric collection task
// void metric_collection_task(void* pvParameters) {
//     SystemMetrics metrics;
//     TickType_t last_wake_time = xTaskGetTickCount();

//     while (1) {
//         // Collect system metrics
//         metrics.timestamp = xTaskGetTickCount();
//         metrics.free_heap = esp_get_free_heap_size();
        
//         // Simulate temperature
//         metrics.cpu_temp = generate_simulated_temperature();
        
//         // Determine temperature status
//         if (metrics.cpu_temp < 10.0f) {
//             metrics.temp_status = 1;  // Low temp
//         } else if (metrics.cpu_temp > 40.0f) {
//             metrics.temp_status = 2;  // High temp
//         } else {
//             metrics.temp_status = 0;  // Normal temp
//         }

//         metrics.cpu_usage = uxTaskGetNumberOfTasks();
//         metrics.task_count = uxTaskGetNumberOfTasks();

//         // Send metrics to queue
//         xQueueSend(metrics_queue, &metrics, portMAX_DELAY);

//         // Sleep for 1 second
//         vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
//     }
// }

// // Serial communication task
// void serial_communication_task(void* pvParameters) {
//     SystemMetrics metrics;
//     CommunicationPacket packet;
//     int bytes_written;

//     while (1) {
//         // Receive metrics from queue
//         if (xQueueReceive(metrics_queue, &metrics, portMAX_DELAY) == pdTRUE) {
//             // Prepare communication packet
//             packet.packet_type = 0x01; // Metrics packet
//             packet.payload_length = sizeof(SystemMetrics);
//             packet.metrics = metrics;
            
//             // Calculate CRC
//             packet.crc = calculate_crc16((uint8_t*)&packet, 
//                                          sizeof(CommunicationPacket) - 2);

//             // Send packet via UART
//             bytes_written = uart_write_bytes(UART_NUM, 
//                                              (const char*)&packet, 
//                                              sizeof(CommunicationPacket));
            
//             // Optional: Add some error checking
//             if (bytes_written != sizeof(CommunicationPacket)) {
//                 // Handle transmission error if needed
//             }
//         }
//     }
// }

// // Real-time control task
// void control_task(void* pvParameters) {
//     SystemMetrics metrics;
//     TickType_t last_wake_time = xTaskGetTickCount();

//     gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

//     while (1) {
//         // Peek at latest metrics without removing from queue
//         if (xQueuePeek(metrics_queue, &metrics, 0) == pdTRUE) {
//             // Control logic based on temperature
//             if (metrics.temp_status == 1) {  // Low temp
//                 // Slow blink pattern
//                 gpio_set_level(LED_GPIO, 1);
//                 vTaskDelay(pdMS_TO_TICKS(500));
//                 gpio_set_level(LED_GPIO, 0);
//                 vTaskDelay(pdMS_TO_TICKS(500));
//             } else if (metrics.temp_status == 2) {  // High temp
//                 // Fast blink pattern
//                 for (int i = 0; i < 5; i++) {
//                     gpio_set_level(LED_GPIO, 1);
//                     vTaskDelay(pdMS_TO_TICKS(100));
//                     gpio_set_level(LED_GPIO, 0);
//                     vTaskDelay(pdMS_TO_TICKS(100));
//                 }
//             } else {
//                 // Normal operation
//                 gpio_set_level(LED_GPIO, 0);
//             }

//             // Additional system health checks
//             if (metrics.free_heap < 10000) {
//                 // Alternate LED pattern for low memory
//                 gpio_set_level(LED_GPIO, 1);
//                 vTaskDelay(pdMS_TO_TICKS(50));
//                 gpio_set_level(LED_GPIO, 0);
//                 vTaskDelay(pdMS_TO_TICKS(50));
//             }
//         }

//         vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(500));
//     }
// }

// void app_main() {
//     // Initialize UART
//     uart_init();

//     // Initialize queues
//     metrics_queue = xQueueCreate(10, sizeof(SystemMetrics));
//     control_queue = xQueueCreate(5, sizeof(uint32_t));

//     // Create tasks
//     xTaskCreate(metric_collection_task, "MetricTask", 4096, 
//                 NULL, METRIC_TASK_PRIORITY, NULL);
    
//     xTaskCreate(serial_communication_task, "SerialCommsTask", 4096, 
//                 NULL, SERIAL_TASK_PRIORITY, NULL);
    
//     xTaskCreate(control_task, "ControlTask", 2048, 
//                 NULL, CONTROL_TASK_PRIORITY, NULL);
// }