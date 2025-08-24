# Sensor Hub

A Raspberry Pi Pico W based alarm using a network of sensors with MQTT connectivity.

## Overview

This project implements a sensor hub that monitors sensor state changes using an MCP23018 I2C GPIO expander and publishes events via MQTT. The system features interrupt-driven alarm triggering, configurable alarm states, and WiFi connectivity for remote monitoring. The sensors can trigger an alarm by sending a signal to one of the expander's pins (configurable with I2C).

## Preparing environment
Sorry if this is weird way but it is the best I came up with.
Environmental variables are loaded during compilation using CMAKE. In order for the other processes to have the access for these variables (like the official raspberry pi extension), the CMAKE `CACHE` is used in combination of running `cmake build` to generate the files.
Reasoning for this is to stop compiling if environmental variables are not prepared. A lot of time debugging was wasted because I forgot to set one environmental variable...

1. Clone the repository
2. Copy `.env.example` to `.env.sh` and update with your actual credentials
3. run `source ./.env.sh`
4. generate build files with the environmental files `cd build && cmake ..`

## Hardware Requirements

- Raspberry Pi Pico W
- MCP23018 I2C GPIO expander
- Sensors that send a digital signal once triggered

## Wiring?

TODO: Document the wiring connections for the MCP23018 and sensors.

![schematic](https://i.imgur.com/MJUmkO6.png)

## Dependencies

### Pico SDK
This project uses the official Raspberry Pi Pico SDK version 2.2.0.

## Building

1. Ensure Pico SDK is installed and configured
2. Install Paho MQTT C client library
3. Clone this repository
4. Create build directory and run the `Compile Project` task

## Configuration

### MQTT Settings
Edit `src/mqtt.h` to configure MQTT broker connection:
- `MQTT_BROKER_HOST`: Broker hostname or IP
- `MQTT_BROKER_PORT`: Broker port (default 1883)
- `MQTT_CLIENT_ID`: Unique client identifier
- `MQTT_USERNAME` and `MQTT_PASSWORD`: Authentication credentials


## MQTT Topics

The system publishes to the following topics:
- `sensor_hub/door`: Door state changes (OPEN/CLOSED)
- `sensor_hub/alarm`: Alarm system state and armed status
- `sensor_hub/heartbeat`: Periodic keep-alive messages
- `sensor_hub/status`: General status messages

Command topic for remote control:
- `sensor_hub/command`: Accepts JSON commands (arm, disarm, status)

## Project Structure

```
src/
├── main.c          # Main application logic
├── main.h          # Main header file
├── mcp23018.c      # MCP23018 GPIO expander driver
├── mcp23018.h      # MCP23018 header file
├── alarm.c         # Alarm state machine implementation
├── alarm.h         # Alarm state machine header
├── mqtt.c          # MQTT client implementation
└── mqtt.h          # MQTT client header
```

## Alarm States

The alarm system implements a finite state machine with the following states:
- `ALARM_STATE_OFF`: System disabled
- `ALARM_STATE_ARMED`: System armed and monitoring
- `ALARM_STATE_TRIGGERED`: Alarm condition detected
- `ALARM_STATE_RESET`: Post-alarm cooldown state

## License

This project is based on the Raspberry Pi Pico SDK examples and follows the BSD-3-Clause license.
