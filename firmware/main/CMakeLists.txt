# Registers the entry-point component. ESP-IDF auto-discovers Kconfig.projbuild
# in this directory and inserts the FleetOS menu into menuconfig.
#
# REQUIRES list:
#   nvs_flash  — non-volatile storage; esp_wifi needs it for credentials cache
#   esp_wifi   — WiFi station driver
#   esp_event  — event loop for WiFi + MQTT callbacks
#   esp_netif  — network interface abstraction (sits above lwIP)
#   mqtt       — esp-mqtt client
#   esp-tls    — TLS underneath esp-mqtt's mqtts:// URIs
#   json       — cJSON for the alive-message payload
#   driver     — gpio for the blink heartbeat
idf_component_register(
    SRCS "main.c" "wifi.c" "mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt esp-tls json driver
)
