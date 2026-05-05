// WiFi STA initialization and connection. Blocks until the first IPv4 lease
// or retries forever — Day 2 doesn't try to handle "WiFi never comes up";
// that's Day 4's failure-mode pass.

#pragma once

#include <stddef.h>

// Initialize WiFi in station mode using credentials from Kconfig
// (CONFIG_FLEETOS_WIFI_SSID / CONFIG_FLEETOS_WIFI_PASSWORD), connect, and
// block until we have an IPv4 address. Safe to call once from app_main.
void fleetos_wifi_start(void);

// Fill `out` with the device's STA MAC formatted as 12 lowercase hex chars
// (no separators), e.g. "ece334a40d4c". Used as the device-id in MQTT topics.
// `out` must hold at least 13 bytes.
void fleetos_wifi_mac_id(char *out, size_t out_len);
