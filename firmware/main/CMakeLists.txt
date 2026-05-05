# Registers the entry-point component. ESP-IDF auto-discovers Kconfig.projbuild
# in this directory and inserts the FleetOS menu into menuconfig.
#
# REQUIRES list:
#   nvs_flash       — non-volatile storage; esp_wifi needs it for PMK cache
#   esp_wifi        — WiFi station driver
#   esp_event       — event loop for WiFi + MQTT callbacks
#   esp_netif       — network interface abstraction (sits above lwIP)
#   mqtt            — esp-mqtt client
#   esp-tls         — TLS underneath mqtts:// + https:// URIs
#   esp_https_ota   — Day 3 HTTPS OTA driver
#   esp_http_client — HTTP client used by esp_https_ota
#   app_update      — esp_ota_* API (partition switching, otadata writes)
#   json            — cJSON for alive payload + cmd parsing
#   driver          — gpio for the blink heartbeat
idf_component_register(
    SRCS "main.c" "wifi.c" "mqtt.c" "ota.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt esp-tls
             esp_https_ota esp_http_client app_update json driver
)
