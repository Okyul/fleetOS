// HTTPS OTA + rollback safety.
//
// fleetos_ota_init must be called once at boot. If the running app was just
// installed via OTA (state == PENDING_VERIFY), it spawns a watchdog task
// that reboots the device after 60s if fleetos_ota_mark_valid hasn't been
// called yet — the bootloader then reverts to the previous good slot.
//
// fleetos_ota_mark_valid should be called once after the app proves it's
// healthy on the network (we wire it to the first successful MQTT alive
// publish in mqtt.c). Safe to call repeatedly; only the first call matters.
//
// fleetos_ota_start kicks off an HTTPS download + flash + reboot.

#pragma once

void fleetos_ota_init(void);
void fleetos_ota_mark_valid(void);
void fleetos_ota_start(const char *url);
