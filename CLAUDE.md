# FleetOS POC — Claude Working Notes

5-day POC: ESP32 over-the-air firmware update demo, ending in a 60–90 second video used for outreach. C on ESP-IDF is the moat — that's the deliberate choice and we don't drift from it.

## Hardware + stack (fixed, do not substitute)

- **Board:** Freenove ESP32 Dev Board (FNK0090). ESP32-WROOM-32, 4 MB flash, dual-core 240 MHz, onboard WiFi.
- **Toolchain:** ESP-IDF v5.3 inside WSL2 Ubuntu on Windows 11. **Not Arduino. Not PlatformIO.**
- **Cloud:** Cloudflare R2 (firmware hosting), HiveMQ Cloud (MQTT broker, TLS).
- **Host CLI:** Python 3 + paho-mqtt.

## Daily milestones (from BUILD_LOG.md — single source of truth for scope)

- **Day 1:** ESP-IDF installed, hardware confirmed working, custom 1Hz blink running.
- **Day 2:** WiFi + MQTT-over-TLS to HiveMQ. "I'm alive" + version on boot. Host script send/receive.
- **Day 3:** Custom partition table + `esp_https_ota`. Laptop publishes URL → device downloads from R2 → flashes → reboots → reports new version.
- **Day 4:** Failure-mode handling (bad URL, network drop, version mismatch). Recordable logs. Demo video shot.
- **Day 5:** README + LinkedIn post + public GitHub.

## DO build

- ESP-IDF C firmware on ESP32.
- WiFi station + MQTT-over-TLS client.
- Custom partition table with two OTA app slots.
- `esp_https_ota` triggered by an MQTT message containing the firmware URL.
- Version reporting back over MQTT after reboot.
- Minimal Python host CLI to publish OTA commands and watch status.
- `firmware-builds/` artifacts uploaded to Cloudflare R2 manually.

## Do NOT build (unless explicitly told)

- Web dashboard / UI. The "fleet manager" is a Python CLI.
- User accounts, auth, RBAC, multi-tenant anything.
- Database. Device state lives in MQTT retained messages or in memory.
- Custom MQTT broker. HiveMQ Cloud only.
- A/B firmware rollout, canary, staged deployment. One device, one update.
- Encrypted firmware, signed firmware, secure boot. Out of scope for the POC.
- Bluetooth, mesh, LoRa, ESP-NOW. WiFi only.
- Multiple board variants. ESP32-WROOM-32 only.
- Arduino fallback. The C-on-ESP-IDF path is the moat — do not erase it.

## Working agreements

- Run commands directly. Don't ask the user to copy-paste output back.
- Windows-side actions (USBIPD attach, driver installs, plugging hardware in) are the user's job — give exact PowerShell or click paths.
- One commit per logical milestone. Imperative present tense: `repo: initialize`, `tooling: install esp-idf`, `device: blink at 1hz`.
- When making a non-obvious choice, drop a one-line reason. The user is learning ESP-IDF as we go.
- Update `BUILD_LOG.md` Day section as we complete milestones — don't wait until end of session.
- Push back when the user tries to skip ahead, expand scope, or reach for Arduino.

## WSL / USBIPD gotchas

- `install.sh` and `export.sh` run inside WSL Ubuntu, never Windows PowerShell.
- Freenove FNK0090 typically uses CH340 USB-serial. Driver: https://www.wch-ic.com/downloads/CH341SER_EXE.html (Windows-side install).
- `usbipd list` → `usbipd bind --busid X-Y` → `usbipd attach --wsl --busid X-Y`. Device shows as `/dev/ttyUSB0` in WSL.
- User must be in `dialout` group inside WSL for serial access without sudo.
- `idf.py flash` hanging is almost always "WSL can't see the USB device" — check `ls /dev/ttyUSB*` first.
- Paths with spaces / parentheses (e.g. `My PC (LAPTOP-M5075A75)`) break ESP-IDF builds. If we hit this, mirror the project to `~/fleetos-poc/` in WSL home and build there.

## Project layout

```
firmware/         ESP-IDF project (runs on ESP32)
host/             Python CLI (added Day 2)
firmware-builds/  Compiled .bin artifacts uploaded to R2 (gitignored)
BUILD_LOG.md      Daily journal — what worked, what broke, what was learned
CLAUDE.md         This file. Scope + working agreements.
```
