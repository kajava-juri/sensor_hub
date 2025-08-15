#include <stdlib.h>
#include <stdio.h>
#include "pico/time.h"
#include "sensor.h"
#include "mcp23018.h"
#include "mqtt.h"
#include "alarm.h"

sensor_manager_t* sensor_manager_init(MQTT_CLIENT_DATA_T *mqtt_ctx, alarm_context_t *alarm_ctx) {
    // Get all sensor config from a file maybe?
    // For now sensors are created here

    sensor_manager_t *manager = calloc(1, sizeof(sensor_manager_t));
    if (!manager) return NULL;

    sensor_config_t front_door = {
        .id = 1,
        .type = SENSOR_TYPE_DOOR,          // This will be cast to uint8_t
        .mcp_pin_mask = GPA7_PIN,
        .name = "Front Door",              // Now only 16 chars max
        .active = true,
        .invert_logic = false,
        .debounce_ms = 0,                  // Now uint16_t
        .last_event_time = 0
    };

    manager->sensors[0] = front_door;
    manager->active_sensor_mask |= front_door.mcp_pin_mask;
    manager->alarm_ctx = alarm_ctx;
    manager->mqtt_ctx = mqtt_ctx;
    manager->sensor_count = 1;

    return manager;
}

void sensor_handle_interrupt(sensor_manager_t *manager, uint8_t intf, uint8_t intcap) {
    if (!manager) return;
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Check each sensor to see if it triggered the interrupt
    for (int i = 0; i < manager->sensor_count; i++) {
        sensor_config_t *sensor = &manager->sensors[i];
        
        if (!sensor->active) continue;
        
        // Check if this sensor's pin caused the interrupt
        if (intf & sensor->mcp_pin_mask) {
            // Get the current state from the interrupt capture register
            bool pin_state = (intcap & sensor->mcp_pin_mask) ? true : false;
            
            // Apply invert logic if needed
            bool sensor_state = sensor->invert_logic ? !pin_state : pin_state;
            
            // Check debouncing
            if (current_time - sensor->last_event_time < sensor->debounce_ms) {
                printf("Sensor %s: debounced (too soon)\n", sensor->name);
                continue;
            }
            
            // Handle different sensor types
            switch ((sensor_type_t)sensor->type) {
                case SENSOR_TYPE_DOOR:
                    printf("Door sensor '%s' %s\n", sensor->name, sensor_state ? "opened" : "closed");
                    
                    // Publish to MQTT
                    mqtt_publish_door_state(*manager->mqtt_ctx, sensor_state);
                    
                    // Update alarm system - only trigger if armed and door opened
                    if (sensor_state && alarm_is_armed(manager->alarm_ctx)) {
                        printf("Door opened while armed - triggering alarm!\n");
                        update_alarm_state(manager->alarm_ctx, EVENT_TRIGGER);
                    } else if (sensor_state) {
                        printf("Door opened while disarmed - no alarm\n");
                    }
                    break;
                    
                case SENSOR_TYPE_WINDOW:
                    printf("Window sensor '%s' %s\n", sensor->name, sensor_state ? "opened" : "closed");
                    // Add window-specific handling here
                    break;
                    
                case SENSOR_TYPE_MOTION:
                    printf("Motion sensor '%s' %s\n", sensor->name, sensor_state ? "motion detected" : "motion cleared");
                    // Add motion-specific handling here
                    break;
                    
                case SENSOR_TYPE_ARM_BUTTON:
                    if (sensor_state) {
                        printf("ARM button '%s' pressed\n", sensor->name);
                        update_alarm_state(manager->alarm_ctx, EVENT_ARM);
                    }
                    break;
                    
                case SENSOR_TYPE_DISARM_BUTTON:
                    if (sensor_state) {
                        printf("DISARM button '%s' pressed\n", sensor->name);
                        update_alarm_state(manager->alarm_ctx, EVENT_DISARM);
                    }
                    break;
                    
                default:
                    printf("Unknown sensor type %d for '%s'\n", sensor->type, sensor->name);
                    break;
            }
        }
    }
}

