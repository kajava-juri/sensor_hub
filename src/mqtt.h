#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include "lwip/apps/mqtt.h"
#include "alarm.h"

// MQTT Configuration
#define MQTT_BROKER_PORT 8883
#define MQTT_CLIENT_ID "sensor_hub_pico"

#define MQTT_KEEP_ALIVE_S 60
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

// Topic definitions
#define SENSOR_ROOT_TOPIC "sensor_hub"
#define MQTT_TOPIC_HEARTBEAT "heartbeat"
#define MQTT_TOPIC_COMMAND "cmd"

#define MQTT_FULL_TOPIC_HEARTBEAT SENSOR_ROOT_TOPIC "/" DEVICE_NAME "/" MQTT_TOPIC_HEARTBEAT
#define MQTT_FULL_TOPIC_COMMAND SENSOR_ROOT_TOPIC "/" DEVICE_NAME "/" MQTT_TOPIC_COMMAND

typedef struct {
    const char *wifi_status;
    const char *mqtt_status;
    uint32_t uptime_ms;
    uint32_t sensor_count;
} system_status_t;

typedef struct MQTT_CLIENT_DATA_T_
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
} MQTT_CLIENT_DATA_T;

MQTT_CLIENT_DATA_T* mqtt_init();
int mqtt_connect(MQTT_CLIENT_DATA_T* mqtt_ctx, char* broker_ip);
static void mqtt_request_cb(void *arg, err_t err);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
void mqtt_publish_door_state(MQTT_CLIENT_DATA_T mqtt_ctx, bool door_state, const char *sensor_id);
void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);
bool mqtt_is_connected(MQTT_CLIENT_DATA_T* mqtt_ctx);
void mqtt_publish_system_status(MQTT_CLIENT_DATA_T* mqtt_ctx, system_status_t* status, alarm_context_t *alarm_ctx);
void mqtt_publish_heartbeat(MQTT_CLIENT_DATA_T *mqtt_ctx, alarm_context_t *alarm_ctx);

#endif // MQTT_H
