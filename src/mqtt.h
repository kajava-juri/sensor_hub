#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include "lwip/apps/mqtt.h"

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
#define MQTT_TOPIC_STATUS "sensor_hub/status"
#define MQTT_TOPIC_DOOR "sensor_hub/door"
#define MQTT_TOPIC_ALARM "sensor_hub/alarm"
#define MQTT_TOPIC_HEARTBEAT "sensor_hub/heartbeat"
#define MQTT_TOPIC_COMMAND "sensor_hub/command"

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
void mqtt_publish_door_state(MQTT_CLIENT_DATA_T mqtt_ctx, bool door_state);
void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

#endif // MQTT_H
