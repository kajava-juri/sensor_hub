#ifndef COMMON_H
#define COMMON_H

#include "lwip/apps/mqtt.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/time.h"
 
typedef struct
{
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    uint8_t data[MQTT_OUTPUT_RINGBUF_SIZE];
    uint8_t topic[100];
    uint32_t len;
    bool newTopic;
    bool stop_client;
    int subscribe_count;
    bool connect_done;
    ip_addr_t mqtt_server_address;
    uint32_t last_disconnect_time;
    uint32_t last_reconnect_attempt;
    bool reconnect_needed;
    uint8_t reconnect_attempts;
} MQTT_CLIENT_DATA_T;

typedef enum {
    ALARM_STATE_ARMED,
    ALARM_STATE_ARMING,
    ALARM_STATE_DISARMED,
    ALARM_STATE_TRIGGERED,
    ALARM_STATE_TRIGGERING
} alarm_state_t;

// Sensor configuration structure - optimized for memory
typedef struct {
    uint8_t id;                    // Unique sensor ID
    uint8_t type;                  // Type of sensor (changed from enum to uint8_t)
    uint8_t mcp_pin_mask;          // Pin mask on MCP23018 (e.g., 0x80 for pin 7)
    char name[16];                 // Human-readable name (reduced from 32 to 16)
    char computer_name[16];        // Computer-readable name (reduced from 32 to 16)
    bool active;                   // Whether this sensor is active
    bool invert_logic;             // Invert pin logic
    uint16_t debounce_ms;          // Debounce time in milliseconds (reduced from uint32_t)
    uint32_t last_event_time;      // Last event timestamp for debouncing
} sensor_config_t;

typedef struct {
    alarm_state_t current_state;
    uint32_t alarm_start_time;
    sensor_config_t* triggered_sensor; // Sensor that triggered the alarm

    struct repeating_timer entry_timer;
    struct repeating_timer exit_timer;
    bool exit_delay_active;
    bool enter_delay_active;
} alarm_context_t;

#endif // COMMON_H