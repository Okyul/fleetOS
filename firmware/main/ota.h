// HTTPS OTA wrapper. Spawns a one-shot task to download and apply firmware
// from the given URL, then reboots the device on success.

#pragma once

// Kick off an OTA from `url` (must be https://). Returns immediately;
// the actual download + apply + reboot happens in a background task.
// The URL is copied internally — caller can free its buffer after this returns.
void fleetos_ota_start(const char *url);
