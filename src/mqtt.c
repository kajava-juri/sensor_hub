#include "mqtt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h" // needed to set hostname
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"
#include "main.h"
#include "alarm.h"

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

mqtt_flags_t mqtt_flags = {0};

MQTT_CLIENT_DATA_T* mqtt_init() {
    MQTT_CLIENT_DATA_T* mqtt=(MQTT_CLIENT_DATA_T*)calloc(1, sizeof(MQTT_CLIENT_DATA_T));
    if (!mqtt) {
        printf("Failed to allocate memory for MQTT client\n");
        return NULL;
    }

    static char client_id_buffer[32];
    strncpy(client_id_buffer, MQTT_CLIENT_ID, sizeof(client_id_buffer) - 1);
    client_id_buffer[sizeof(client_id_buffer) - 1] = '\0';
    mqtt->mqtt_client_info.client_id = client_id_buffer;
    mqtt->mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
    mqtt->newTopic=false;
    #if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
        mqtt->mqtt_client_info.client_user = MQTT_USERNAME;
        mqtt->mqtt_client_info.client_pass = MQTT_PASSWORD;
    #else
        mqtt->mqtt_client_info.client_user = NULL;
        mqtt->mqtt_client_info.client_pass = NULL;
    #endif
        static char will_topic[sizeof(MQTT_FULL_TOPIC_HEARTBEAT)];
        snprintf(will_topic, sizeof(will_topic), MQTT_FULL_TOPIC_HEARTBEAT);
        will_topic[sizeof(will_topic) - 1] = '\0';
        mqtt->mqtt_client_info.will_topic = will_topic;
        mqtt->mqtt_client_info.will_msg = MQTT_WILL_MSG;
        mqtt->mqtt_client_info.will_qos = MQTT_WILL_QOS;
        mqtt->mqtt_client_info.will_retain = true;
    #if LWIP_ALTCP && LWIP_ALTCP_TLS
        // TLS enabled
        printf("Using TLS\n");
    #ifdef MQTT_CERT_INC
        printf("Using client certificates\n");
        static const uint8_t ca_cert[] = TLS_ROOT_CERT;
        static const uint8_t client_key[] = TLS_CLIENT_KEY;
        static const uint8_t client_cert[] = TLS_CLIENT_CERT;
        // This confirms the indentity of the server and the client
        mqtt->mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
                client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
        
        // Note: Certificate has CN=localhost but we're connecting to IP address
        // This might cause hostname verification issues
        printf("TLS config created: %p\n", mqtt->mqtt_client_info.tls_config);
    #if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
        WARN_printf("Warning: tls without verification is insecure\n");
    #endif
    #else
        printf("Using TLS without client certificates\n");
        mqtt->mqtt_client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
        WARN_printf("Warning: tls without a certificate is insecure\n");
    #endif
    #else
        printf("Not using TLS\n");
    #endif
    
    return mqtt;
}

int mqtt_connect(MQTT_CLIENT_DATA_T* mqtt_ctx, char* broker_ip) {
    
    // We are not in a callback so locking is needed when calling lwip
    // Make a DNS request for the MQTT server IP address
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &mqtt_ctx->mqtt_server_address, dns_found, mqtt_ctx);
    cyw43_arch_lwip_end();
    mqtt_ctx->mqtt_client_inst = mqtt_client_new();
    
    if (!mqtt_ctx->mqtt_client_inst) {
        printf("mqtt client inst error\n");
        return -1;
    }

    printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&mqtt_ctx->mqtt_server_address));
    
    if (err == ERR_OK) {
        cyw43_arch_lwip_begin();

        err_t mqtt_err = mqtt_client_connect(mqtt_ctx->mqtt_client_inst, &mqtt_ctx->mqtt_server_address, MQTT_BROKER_PORT, mqtt_connection_cb, mqtt_ctx, &mqtt_ctx->mqtt_client_info);
        if (err != ERR_OK) {
            printf("mqtt_client_connect failed: %d\n", err);
            cyw43_arch_lwip_end();
            return -1;
        }
    }


#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(mqtt_ctx->mqtt_client_inst->conn), MQTT_SERVER);
    printf("TLS hostname set to: %s\n", MQTT_SERVER);
#endif

    mqtt_set_inpub_callback(mqtt_ctx->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, mqtt_ctx);
    cyw43_arch_lwip_end();

    return 0;
}

static void mqtt_request_cb(void *arg, err_t err) {
  MQTT_CLIENT_DATA_T* mqtt_client = ( MQTT_CLIENT_DATA_T*)arg;
 
  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" request cb: err %d\n", mqtt_client->mqtt_client_info.client_id, (int)err));
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T *mqtt_client = (MQTT_CLIENT_DATA_T *)arg;
    LWIP_UNUSED_ARG(client);

    LWIP_PLATFORM_DIAG(("MQTT client \"%s\" connection cb: status %d\n", mqtt_client->mqtt_client_info.client_id, (int)status));

    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT connected!\n");
        mqtt_client->connect_done = true;
        // Indicate online
        if(mqtt_client->mqtt_client_info.will_topic) {
            mqtt_publish(mqtt_client->mqtt_client_inst, mqtt_client->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, mqtt_request_cb, mqtt_client);
        }

        mqtt_sub_unsub(client, "sensor/commands", 0, mqtt_request_cb, arg, 1);
        mqtt_sub_unsub(client, "sensor/arm", 0, mqtt_request_cb, arg, 1);
        mqtt_sub_unsub(client, "sensor/disarm", 0, mqtt_request_cb, arg, 1);
    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!mqtt_client->connect_done) {
            panic("MQTT connection failed");
        }
    }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* mqtt_client = (MQTT_CLIENT_DATA_T*)arg;
    LWIP_UNUSED_ARG(data);
 
    strncpy(mqtt_client->data, data, len);
    mqtt_client->len=len;
    mqtt_client->data[len]='\0';
    
    mqtt_client->newTopic=true;
 
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  MQTT_CLIENT_DATA_T* mqtt_client = (MQTT_CLIENT_DATA_T*)arg;
  strcpy(mqtt_client->topic, topic);
}

void mqtt_publish_door_state(MQTT_CLIENT_DATA_T *mqtt_ctx, bool door_state, const char *sensor_id) {
    char topic[100];
    snprintf(topic, sizeof(topic), "%s/%s/door/%s/%s", SENSOR_ROOT_TOPIC, DEVICE_NAME, sensor_id, door_state ? "open" : "closed");

    char message[50];
    snprintf(message, sizeof(message), "{\"state\": \"%s\"}", door_state ? "open" : "closed");

    err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, (const u8_t *)message, strlen(message), 0, 0, mqtt_request_cb, mqtt_ctx);
    if (err != ERR_OK) {
        printf("mqtt_publish failed: %d\n", err);
    }
}

void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
    } else {
        panic("dns request failed");
    }
}

void mqtt_publish_heartbeat(MQTT_CLIENT_DATA_T *mqtt_ctx, alarm_context_t *alarm_ctx) {
    if (mqtt_ctx == NULL || mqtt_ctx->mqtt_client_inst == NULL) {
        printf("MQTT client not initialized\n");
        return;
    }

    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    char message[512];
    snprintf(message, sizeof(message), 
        "{"
        "\"status\":\"online\","
        "\"timestamp\":%lu,"
        "\"uptime_ms\":%lu,"
        "\"alarm_state\": \"%s\","
        "\"wifi_connected\":true,"
        "\"mqtt_connected\":true,"
        "\"free_heap\":0,"
        "\"version\":\"1.0.0\""
        "}",
        current_time,
        current_time,
        alarm_state_to_string(alarm_ctx->current_state)
    );

    err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, MQTT_FULL_TOPIC_HEARTBEAT, (const u8_t *)message, strlen(message), 0, 0, mqtt_request_cb, mqtt_ctx);
    if (err != ERR_OK) {
        printf("mqtt_publish failed: %d\n", err);
    }
}

void mqtt_publish_system_status(MQTT_CLIENT_DATA_T* mqtt_ctx, system_status_t* status, alarm_context_t *alarm_ctx) {
    if (!mqtt_is_connected(mqtt_ctx)) return;
    
    char message[512];
    snprintf(message, sizeof(message),
        "{"
        "\"wifi_status\":\"%s\","
        "\"mqtt_status\":\"%s\","
        "\"uptime_ms\":%lu,"
        "\"sensor_count\":%lu,"
        "\"timestamp\":%lu,"
        "\"alarm_state\":\"%s\""
        "}",
        status->wifi_status,
        status->mqtt_status,
        status->uptime_ms,
        status->sensor_count,
        to_ms_since_boot(get_absolute_time()),
        alarm_state_to_string(alarm_ctx->current_state)
    );

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/status", SENSOR_ROOT_TOPIC, DEVICE_NAME);

    err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, 
                            message, strlen(message), MQTT_PUBLISH_QOS, false, 
                            mqtt_request_cb, mqtt_ctx);
    
    if (err != ERR_OK) {
        printf("Failed to publish system status: %d\n", err);
    }
}

bool mqtt_is_connected(MQTT_CLIENT_DATA_T* mqtt_ctx) {
    return mqtt_ctx && mqtt_ctx->mqtt_client_inst && mqtt_ctx->connect_done;
}

void mqtt_publish_alarm_state(MQTT_CLIENT_DATA_T* mqtt_ctx, alarm_context_t* alarm_ctx) {
    if (!mqtt_is_connected(mqtt_ctx)) return;

    char message[256];
    // snprintf(message, sizeof(message),
    //     "{"
    //     "\"state\":\"%s\","
    //     "\"timestamp\":%lu"
    //     "}",
    //     alarm_state_to_string(alarm_ctx->current_state),
    //     to_ms_since_boot(get_absolute_time())
    // );

    char topic[128];
    // snprintf(topic, sizeof(topic), "%s/%s/alarm/status", SENSOR_ROOT_TOPIC, DEVICE_NAME);

    // err_t err = mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, 
    //                         message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, 
    //                         mqtt_request_cb, mqtt_ctx);

    if(alarm_ctx->current_state == ALARM_STATE_TRIGGERED) {
        snprintf(topic, sizeof(topic), "%s/%s/alarm/triggered", SENSOR_ROOT_TOPIC, DEVICE_NAME);
        snprintf(message, sizeof(message), 
            "{"
            "\"triggered_by\":\"%s\","
            "\"timestamp\":%lu"
            "}",
            alarm_ctx->triggered_sensor->computer_name,
            to_ms_since_boot(get_absolute_time())
        );
        mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, 
            message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, 
            mqtt_request_cb, mqtt_ctx);
    }
    else if(alarm_ctx->current_state == ALARM_STATE_DISARMED) {
        snprintf(topic, sizeof(topic), "%s/%s/alarm/disarmed", SENSOR_ROOT_TOPIC, DEVICE_NAME);
        snprintf(message, sizeof(message), 
            "{"
            "\"timestamp\":%lu"
            "}",
            to_ms_since_boot(get_absolute_time())
        );
        mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, 
            message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, 
            mqtt_request_cb, mqtt_ctx);
    }
    else if(alarm_ctx->current_state == ALARM_STATE_ARMED) {
        snprintf(topic, sizeof(topic), "%s/%s/alarm/armed", SENSOR_ROOT_TOPIC, DEVICE_NAME);
        snprintf(message, sizeof(message), 
            "{"
            "\"timestamp\":%lu"
            "}",
            to_ms_since_boot(get_absolute_time())
        );
        mqtt_publish(mqtt_ctx->mqtt_client_inst, topic, 
            message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, 
            mqtt_request_cb, mqtt_ctx);
    }
    if (err != ERR_OK) {
        printf("Failed to publish alarm state: %d\n", err);
    }
}

void mqtt_check_and_publish(MQTT_CLIENT_DATA_T* mqtt_ctx, alarm_context_t* alarm_ctx) {
   if (!mqtt_is_connected(mqtt_ctx)) {
       return;  // Skip if MQTT not connected
   }
   
   uint32_t current_time = to_ms_since_boot(get_absolute_time());
   
   // Check alarm state changes
   if (mqtt_flags.alarm_state_changed) {
       // Publish: /sensor_hub/<device>/alarm/status
       // Body: {"state": "armed|disarmed|triggered", "triggered_by": "button|door|timeout", "timestamp": 123456}
       mqtt_publish_alarm_state(mqtt_ctx, alarm_ctx);
       
       // Clear flag after publishing
       mqtt_flags.alarm_state_changed = false;
       mqtt_flags.last_published_alarm_state = alarm_ctx->current_state;
   }
   
   // Check door state changes
   if (mqtt_flags.door_state_changed) {
       // Publish: /sensor_hub/<device>/door/<sensor_id>/status
       // Body: {"state": "open|closed", "timestamp": 123456}
       // Also publish: /sensor_hub/<device>/door/<sensor_id>/open (or /closed)
       //mqtt_publish_door_event(mqtt_ctx, mqtt_flags.last_door_sensor, mqtt_flags.last_door_state);
       mqtt_publish_door_state(mqtt_ctx, mqtt_flags.last_door_state, mqtt_flags.last_door_sensor->computer_name);

       mqtt_flags.door_state_changed = false;
   }
   
   // Check motion sensor changes (if you add them later)
   if (mqtt_flags.motion_state_changed) {
       // Publish: /sensor_hub/<device>/motion/<sensor_id>/status
       // Body: {"state": "detected|clear", "timestamp": 123456}
       mqtt_flags.motion_state_changed = false;
   }
   
   // Check button events
   if (mqtt_flags.button_pressed) {
       // Publish: /sensor_hub/<device>/button/<button_id>/pressed
       // Body: {"button": "arm|disarm|reset", "timestamp": 123456}
       mqtt_flags.button_pressed = false;
   }
   
   // Periodic heartbeat/status
   if (current_time - mqtt_flags.last_heartbeat_time > HEARTBEAT_INTERVAL_MS) {
       // Publish: /sensor_hub/<device>/system/heartbeat
       // Body: {"uptime": 123456, "state": "armed", "wifi_rssi": -45, "free_memory": 1024}
       mqtt_publish_heartbeat(mqtt_ctx, alarm_ctx);
       mqtt_flags.last_heartbeat_time = current_time;
   }
   
   // Error/diagnostic messages (optional)
   if (mqtt_flags.error_occurred) {
       // Publish: /sensor_hub/<device>/system/error
       // Body: {"error": "i2c_timeout", "timestamp": 123456}
       mqtt_flags.error_occurred = false;
   }
}