// esp-mqtt client wrapper. Connects to HiveMQ Cloud over TLS using credentials
// from Kconfig, publishes a retained "alive" message on every connect,
// subscribes to fleet/<device-id>/cmd for incoming commands, and exposes a
// status channel that ota.c uses to report progress.
//
// Topic structure:
//   fleet/<device-id>/alive   -- retained: device snapshot (version, uptime,
//                                heap, rssi). Republished on connect and
//                                every 30s by the heartbeat task.
//   fleet/<device-id>/cmd     -- inbound: {"url":"https://..."} for OTA
//   fleet/<device-id>/status  -- retained: device state machine
//                                (ready / downloading / rebooting / error)

#pragma once

// Start the MQTT client. Non-blocking — the client runs in its own task,
// reconnects automatically. `device_id` and `firmware_version` are borrowed
// pointers; they must outlive the program (`static const` is fine).
void fleetos_mqtt_start(const char *device_id, const char *firmware_version);

// Spawn the heartbeat task: republishes the alive message every 30s with
// fresh uptime/heap/rssi so the host can show liveness without waiting for
// a boot. Call once after fleetos_mqtt_start.
void fleetos_mqtt_heartbeat_start(void);

// Publish a retained status update. `state` is required (e.g. "ready",
// "downloading", "rebooting", "error"). `detail` may be NULL; if non-NULL
// it's added as a "detail" field in the JSON payload. Safe to call before
// MQTT has connected — the publish queues internally.
void fleetos_mqtt_publish_status(const char *state, const char *detail);
