#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "common.h"

#define MAX_SENSORS 4

typedef enum {
    SENSOR_TYPE_DOOR,
    SENSOR_TYPE_WINDOW,
    SENSOR_TYPE_MOTION,
    SENSOR_TYPE_SMOKE,
    SENSOR_TYPE_ARM_BUTTON,
    SENSOR_TYPE_DISARM_BUTTON,
} sensor_type_t;

// Sensor event types
typedef enum {
    SENSOR_EVENT_DOOR_OPENED,
    SENSOR_EVENT_DOOR_CLOSED,
    SENSOR_EVENT_MOTION_DETECTED,
    SENSOR_EVENT_MOTION_CLEARED,
    SENSOR_EVENT_SMOKE_DETECTED,
    SENSOR_EVENT_SMOKE_CLEARED,
    SENSOR_EVENT_BUTTON_PRESSED,
    SENSOR_EVENT_BUTTON_RELEASED,
} sensor_event_t;

// Sensor manager structure
typedef struct {
    sensor_config_t sensors[MAX_SENSORS];
    uint8_t sensor_count;
    uint8_t active_sensor_mask;    // Bitmask of which MCP pins have sensors
    MQTT_CLIENT_DATA_T* mqtt_ctx;
    alarm_context_t* alarm_ctx;    // Changed to pointer to work with current code
} sensor_manager_t;

// Function prototypes
sensor_manager_t* sensor_manager_init(MQTT_CLIENT_DATA_T *mqtt_ctx, alarm_context_t *alarm_ctx);
void sensor_handle_interrupt(sensor_manager_t *manager, uint8_t intf, uint8_t intcap);

#endif // SENSOR_H
