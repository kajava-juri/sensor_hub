# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Use cached variables with environment fallback
set(WIFI_SSID_DEFAULT "$ENV{WIFI_SSID}")
set(WIFI_PASSWORD_DEFAULT "$ENV{WIFI_PASSWORD}")
set(MQTT_SERVER_DEFAULT "$ENV{MQTT_SERVER}")

# Create cache variables that persist between CMake runs
set(WIFI_SSID "${WIFI_SSID_DEFAULT}" CACHE STRING "WiFi SSID")
set(WIFI_PASSWORD "${WIFI_PASSWORD_DEFAULT}" CACHE STRING "WiFi Password") 
set(MQTT_SERVER "${MQTT_SERVER_DEFAULT}" CACHE STRING "MQTT Server Address")


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(sensor_hub C CXX ASM)

# Use environment variables with fallback defaults for development
if(DEFINED ENV{MQTT_SERVER})
    set(MQTT_SERVER $ENV{MQTT_SERVER})
else()
    set(MQTT_SERVER "192.168.1.223")
    message(WARNING "Using default MQTT_SERVER. Set MQTT_SERVER environment variable for production.")
endif()

if(DEFINED ENV{WIFI_SSID} AND NOT "$ENV{WIFI_SSID}" STREQUAL "")
    set(WIFI_SSID $ENV{WIFI_SSID})
else()
    set(WIFI_SSID "YOUR_WIFI_SSID")
    message(FATAL_ERROR "WIFI_SSID environment variable is required and cannot be empty")
endif()

if(DEFINED ENV{WIFI_PASSWORD} AND NOT "$ENV{WIFI_PASSWORD}" STREQUAL "")
    set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
else()
    set(WIFI_PASSWORD "YOUR_WIFI_PASSWORD")
    message(FATAL_ERROR "WIFI_PASSWORD environment variable is required and cannot be empty")
endif()

if (NOT MQTT_CERT_PATH)
    set(MQTT_CERT_PATH /opt/home-sec/home-sec-proj/certs)
endif()

if (NOT MQTT_CERT_INC)
    set(MQTT_CERT_INC mqtt_client.inc)
endif()

add_executable(sensor_hub
        src/main.c
        src/mcp23018.c
        src/alarm.c
        src/mqtt.c
        src/sensor.c
        src/buttons.c
        src/mqtt_cmd.c
        )
# pull in common dependencies and additional i2c hardware support
target_link_libraries(sensor_hub 
                        pico_stdlib 
                        hardware_i2c
                        pico_cyw43_arch_lwip_threadsafe_background
                        pico_lwip_mqtt
                        pico_mbedtls
                        pico_lwip_mbedtls
                        )
                        
target_include_directories(sensor_hub PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for common lwipopts or any other standard includes
        )

target_compile_definitions(sensor_hub PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    MQTT_SERVER=\"${MQTT_SERVER}\"
    PICO_PANIC_FUNCTION=detailed_panic
    )

if (EXISTS "${MQTT_CERT_PATH}/${MQTT_CERT_INC}")
    target_compile_definitions(sensor_hub PRIVATE
        MQTT_CERT_INC=\"${MQTT_CERT_INC}\" # contains the tls certificates for MQTT_SERVER needed by the client
        ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED
        )
    target_include_directories(sensor_hub PRIVATE
        ${MQTT_CERT_PATH}
        )
endif()

# Optional MQTT authentication
if(DEFINED ENV{MQTT_USERNAME} AND NOT "$ENV{MQTT_USERNAME}" STREQUAL "")
    set(MQTT_USERNAME $ENV{MQTT_USERNAME})
endif()

if(DEFINED ENV{MQTT_PASSWORD} AND NOT "$ENV{MQTT_PASSWORD}" STREQUAL "")
    set(MQTT_PASSWORD $ENV{MQTT_PASSWORD})
endif()

if (MQTT_USERNAME AND MQTT_PASSWORD)
    target_compile_definitions(sensor_hub PRIVATE
        MQTT_USERNAME=\"${MQTT_USERNAME}\"
        MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
    )
endif()

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1


# create map/bin/hex file etc.
pico_add_extra_outputs(sensor_hub)


set_source_files_properties(
    ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
    PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-result"
    )