#include "mqtt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/dns.h"
#include "main.h"
#include "alarm.h"

// Simple JSON parsing functions (lightweight alternative to cJSON)
static const char* find_json_value(const char* json, const char* key) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return NULL;
    
    pos += strlen(search_key);
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t') pos++;
    
    // Skip opening quote if present
    if (*pos == '"') pos++;
    
    return pos;
}

static int extract_json_string(const char* json, const char* key, char* output, size_t output_size) {
    const char* start = find_json_value(json, key);
    if (!start) return -1;
    
    const char* end = strchr(start, '"');
    if (!end) {
        // Handle case where value is not quoted (shouldn't happen for strings)
        end = strchr(start, ',');
        if (!end) end = strchr(start, '}');
        if (!end) return -1;
    }
    
    size_t len = end - start;
    if (len >= output_size) len = output_size - 1;
    
    strncpy(output, start, len);
    output[len] = '\0';
    
    return 0;
}

static long extract_json_number(const char* json, const char* key) {
    const char* start = find_json_value(json, key);
    if (!start) return -1;
    
    return strtol(start, NULL, 10);
}

void mqtt_handle_command(MQTT_CLIENT_DATA_T* mqtt_ctx, alarm_context_t* alarm_ctx, const char* command_json) {
    if (!mqtt_ctx || !alarm_ctx || !command_json) {
        printf("Invalid parameters for command handling\n");
        return;
    }
    
    printf("Received command: %s\n", command_json);
    
    // Parse command
    char command[32] = {0};
    char source[32] = {0};
    
    if (extract_json_string(command_json, "command", command, sizeof(command)) != 0) {
        printf("Failed to parse command field\n");
        mqtt_publish_command_response(mqtt_ctx, "error", "Invalid command format", NULL);
        return;
    }
    
    // Source is optional
    extract_json_string(command_json, "source", source, sizeof(source));

    printf("Processing command: '%s' from source: '%s'\n",
           command, source[0] ? source : "unknown");

    // Handle commands
    if (strcmp(command, "arm") == 0) {
        if (alarm_ctx->current_state == ALARM_STATE_DISARMED) {
            update_alarm_state(alarm_ctx, EVENT_EXIT_DELAY);
            mqtt_publish_command_response(mqtt_ctx, "success", "Arming initiated with exit delay", command);
            printf("Remote ARM command executed - starting exit delay\n");
        } else if (alarm_ctx->current_state == ALARM_STATE_ARMED) {
            mqtt_publish_command_response(mqtt_ctx, "warning", "System already armed", command);
            printf("ARM command ignored - system already armed\n");
        } else {
            mqtt_publish_command_response(mqtt_ctx, "error", "Cannot arm - system in invalid state", command);
            printf("ARM command failed - current state: %s\n", alarm_state_to_string(alarm_ctx->current_state));
        }
    }
    else if (strcmp(command, "disarm") == 0) {
        if (alarm_ctx->current_state != ALARM_STATE_DISARMED) {
            update_alarm_state(alarm_ctx, EVENT_DISARM);
            mqtt_publish_command_response(mqtt_ctx, "success", "System disarmed", command);
            printf("Remote DISARM command executed\n");
        } else {
            mqtt_publish_command_response(mqtt_ctx, "warning", "System already disarmed", command);
            printf("DISARM command ignored - system already disarmed\n");
        }
    }
    else if (strcmp(command, "status") == 0) {
        mqtt_publish_status_response(mqtt_ctx, alarm_ctx);
        printf("Status request processed\n");
    }
    else if (strcmp(command, "reset") == 0) {
        if (alarm_ctx->current_state != ALARM_STATE_DISARMED) {
            update_alarm_state(alarm_ctx, EVENT_RESET);
            mqtt_publish_command_response(mqtt_ctx, "success", "System reset to armed state", command);
            printf("Remote RESET command executed\n");
        } else {
            mqtt_publish_command_response(mqtt_ctx, "warning", "System already disarmed", command);
            printf("RESET command ignored - system already disarmed\n");
        }
    }
    else {
        mqtt_publish_command_response(mqtt_ctx, "error", "Unknown command", command);
        printf("Unknown command received: %s\n", command);
    }
}

void mqtt_publish_command_response(MQTT_CLIENT_DATA_T* mqtt_ctx, const char* status, const char* message, const char* command) {
    if (!mqtt_is_connected(mqtt_ctx)) {
        printf("Cannot publish command response - MQTT not connected\n");
        return;
    }
    
    char response_topic[128];
    char response_message[256];
    
    snprintf(response_topic, sizeof(response_topic), "%s/%s/cmd/response", SENSOR_ROOT_TOPIC, DEVICE_NAME);
    
    snprintf(response_message, sizeof(response_message),
        "{"
        "\"status\":\"%s\","
        "\"message\":\"%s\","
        "\"command\":\"%s\","
        "\"timestamp\":%lu"
        "}",
        status,
        message,
        command ? command : "",
        to_ms_since_boot(get_absolute_time())
    );
    
    err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, response_topic,
                            response_message, strlen(response_message),
                            MQTT_PUBLISH_QOS, false,
                            mqtt_request_cb, mqtt_ctx);
    
    if (err != ERR_OK) {
        printf("Failed to publish command response: %d\n", err);
    } else {
        printf("Published command response: %s\n", response_message);
    }
}

void mqtt_publish_status_response(MQTT_CLIENT_DATA_T* mqtt_ctx, alarm_context_t* alarm_ctx) {
    if (!mqtt_is_connected(mqtt_ctx)) {
        printf("Cannot publish status response - MQTT not connected\n");
        return;
    }
    
    char status_topic[128];
    char status_message[512];
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    snprintf(status_topic, sizeof(status_topic), "%s/%s/status/response", SENSOR_ROOT_TOPIC, DEVICE_NAME);
    
    snprintf(status_message, sizeof(status_message),
        "{"
        "\"alarm_state\":\"%s\","
        "\"uptime_ms\":%lu,"
        "\"wifi_connected\":true,"
        "\"mqtt_connected\":true,"
        "\"exit_delay_active\":%s,"
        "\"entry_delay_active\":%s,"
        "\"timestamp\":%lu,"
        "\"version\":\"1.0.0\""
        "}",
        alarm_state_to_string(alarm_ctx->current_state),
        current_time,
        alarm_ctx->exit_delay_active ? "true" : "false",
        alarm_ctx->enter_delay_active ? "true" : "false",
        current_time
    );
    
    err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, status_topic,
                            status_message, strlen(status_message),
                            MQTT_PUBLISH_QOS, false,
                            mqtt_request_cb, mqtt_ctx);
    
    if (err != ERR_OK) {
        printf("Failed to publish status response: %d\n", err);
    } else {
        printf("Published status response: %s\n", status_message);
    }
}