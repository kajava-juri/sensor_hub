#ifndef CONFIG_FALLBACK_H
#define CONFIG_FALLBACK_H

// Fallback definitions for IntelliSense (for me it did not see the cmake pre-defines)
#ifndef MQTT_SERVER
    #define MQTT_SERVER "192.168.1.223"
#endif

#ifndef WIFI_SSID
    #define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef DEVICE_NAME
    #define DEVICE_NAME "sensor_hub"
#endif

#endif // CONFIG_FALLBACK_H