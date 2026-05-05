// esp-mqtt client wrapper. Connects to HiveMQ Cloud over TLS using credentials
// from Kconfig, publishes a retained "alive" message with version + MAC on
// every connect, and subscribes to fleet/<device-id>/cmd for incoming
// commands. Day 3 will dispatch commands to the OTA module.

#pragma once

// Start the MQTT client and return immediately. The client runs forever in
// its own task, reconnects automatically, and re-publishes alive on each
// reconnect. `device_id` and `firmware_version` are borrowed pointers — they
// must live for the lifetime of the program (`static`/`const` is fine).
void fleetos_mqtt_start(const char *device_id, const char *firmware_version);
