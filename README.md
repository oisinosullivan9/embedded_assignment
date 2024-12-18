# ESP32 Temperature Monitoring System
CS4447 Embedded Systems Project
- Oisin O Sullivan - 22368493

## Overview
This project is a real-time Internet of Things (IoT) application designed for monitoring temperature using an ESP32 microcontroller. It was developed using the ESP-IDF framework with the PlatformIO extension for Visual Studio Code.

## Features
- Temperature monitoring using an LM35 sensor.
- Real-time control with an LED breathing pattern.
- Wi-Fi communication of temperature data to a Python collector (UDP).
- FreeRTOS-based task management and inter-task communication.
- Resilience to network failures with buffered data handling.

## System Components
- **ESP32 Microcontroller**: Executes real-time tasks and communicates with the collector.
- **LM35 Temperature Sensor**: Provides analog temperature data.
- **Python Collector Script**: Receives temperature data via UDP and logs it.

## Setup and Installation

### Prerequisites
- ESP-IDF framework installed ([Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp-idf/)).
- PlatformIO extension in Visual Studio Code.
- Python 3.x installed.

### Setup

1. Clone the repository:
    ```bash
    git clone https://github.com/oisinosullivan9/embedded_assignment.git
    cd ./embedded_assignment/
    ```

2. Update ESP32 firmware configuration:
    - Replace `WIFI_SSID` and `WIFI_PASSWORD` with your Wi-Fi credentials.
    - Replace `COLLECTOR_IP` with your laptop's IP address.

3. Build and flash the ESP32 firmware:
    ```bash
    pio run -t upload
    ```

4. Set up the Python collector:
    - Update `HOST` in the `collector.py` script with your laptop’s IP address.
    - Run the script:
      ```bash
      python wifi_ollector.py
      ```

5. Power up the ESP32 and start monitoring.

## Task Breakdown

### Data Acquisition Task
- Reads temperature data from the LM35 sensor.
- Converts analog voltage to Celsius and queues the data.

### Network Agent Task
- Sends temperature data over UDP to the Python collector.
- Handles retries on network failures.

### LED Pattern Task
- Controls an LED to implement a dynamic breathing pattern.
- Operates independently of the network task.

## Communication Protocol
- **Transport**: UDP over Wi-Fi.
- **Data Format**: JSON-like string containing temperature and timestamp.
- **Resilience**: Retries for failed transmissions and buffers unsent data.

## Future Improvements
- Add support for additional sensors (e.g., humidity, pressure).
- Implement TLS encryption for secure data transmission.
