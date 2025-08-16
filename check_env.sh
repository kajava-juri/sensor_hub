#!/bin/bash
# Check if required environment variables are set

if [ -z "$WIFI_SSID" ] || [ -z "$WIFI_PASSWORD" ]; then
    echo "❌ Error: Environment variables not set."
    echo "Please run: source .env.sh"
    exit 1
else
    echo "✓ Environment variables are set"
    echo "  WIFI_SSID: $WIFI_SSID"
    echo "  MQTT_SERVER: ${MQTT_SERVER:-not set}"
fi
