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

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

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
        static char will_topic[sizeof(MQTT_TOPIC_HEARTBEAT)];
        strncpy(will_topic, MQTT_TOPIC_HEARTBEAT, sizeof(will_topic) - 1);
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
        // Indicate online
        if(mqtt_client->mqtt_client_info.will_topic) {
            mqtt_publish(mqtt_client->mqtt_client_inst, mqtt_client->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, mqtt_request_cb, mqtt_client);
        }

        mqtt_sub_unsub(client, "sensor/commands", 0, mqtt_request_cb, arg, 1);
        mqtt_sub_unsub(client, "sensor/arm", 0, mqtt_request_cb, arg, 1);
        mqtt_sub_unsub(client, "sensor/disarm", 0, mqtt_request_cb, arg, 1);
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

void mqtt_publish_door_state(MQTT_CLIENT_DATA_T mqtt_ctx, bool door_state) {
    char topic[100];
    snprintf(topic, sizeof(topic), "%s/%s", MQTT_TOPIC_DOOR, door_state ? "open" : "closed");
    
    char message[50];
    snprintf(message, sizeof(message), "{\"state\": \"%s\"}", door_state ? "open" : "closed");

    err_t err = mqtt_publish(mqtt_ctx.mqtt_client_inst, topic, (const u8_t *)message, strlen(message), 0, 0, mqtt_request_cb, &mqtt_ctx);
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
