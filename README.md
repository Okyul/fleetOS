# FleetOS

Proof-of-concept over-the-air firmware update pipeline for ESP32 devices. Push a URL from your laptop, watch a device on your desk pull new firmware over HTTPS, swap partitions, reboot, and report the new version back — all in under a minute.

C on ESP-IDF, MQTT-over-TLS to a hosted broker, signed-by-CA HTTPS pulls from object storage. No vendor SDK, no binary blobs, no web dashboard hiding the moving parts.

## Pipeline

```
  laptop                   HiveMQ Cloud                Cloudflare R2
 ┌──────┐  fleet/<id>/cmd  ┌──────────┐               ┌────────────┐
 │ host │ ───────────────▶ │  broker  │               │ firmware   │
 │ CLI  │                  │   TLS    │               │ .bin files │
 └──────┘                  └──────────┘               └────────────┘
                                │ ▲                          ▲
                                │ │ retained alive+status    │ HTTPS GET
                                │ │ heartbeat every 30s      │
                                ▼ │                          │
                         ┌────────────────┐                  │
                         │  ESP32 device  │ ─────────────────┘
                         │  (1 of N)      │  esp_https_ota
                         └────────────────┘
```

With R2 creds in `host/.env`, the whole demo is two commands:

```bash
./tools/release.sh 4.0.0 50              # bump main.c, build, upload to R2
python host/fleetctl.py ota <device-id> 4.0.0   # publish URL via MQTT
```

In ~25 seconds the device downloads the .bin, writes to the inactive OTA slot, updates otadata, reboots, marks itself valid (or rolls back if it can't get back on the broker within 60s), and republishes alive with `version=4.0.0`. Watch live in another terminal: `python host/fleetctl.py status`.

Without R2 creds the manual flow still works — `release.sh` prints the binary path, you drag-drop it into Cloudflare's R2 console, and use `fleetctl cmd <device-id> '{"url":"..."}'` to publish.

## What's actually shipped

- **Two-slot OTA** with rollback safety. `firmware/partitions.csv` defines `ota_0` + `ota_1` (1.5 MB each). On boot, `fleetos_ota_init` checks the current slot's verification state. A freshly-OTA'd image is `PENDING_VERIFY`; it must call `esp_ota_mark_app_valid_cancel_rollback()` (wired to the first successful MQTT publish) within 60s or the bootloader reverts to the previous good slot. Bricking is not a failure mode.
- **TLS to HiveMQ + R2** via ESP-IDF's bundled CA store (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`). No pinned certs to rotate — Let's Encrypt + Cloudflare can swap roots and the device keeps working.
- **Three retained MQTT topics** per device (`fleet/<device-id>/...`):
  - `alive` — `{"version","device_id","uptime_ms","free_heap","rssi"}`. Republished every 30s by the heartbeat task.
  - `status` — state machine (`ready` / `downloading` / `rebooting` / `error`). Last value retained so subscribers see current state on connect.
  - `cmd` — host → device JSON commands. Currently `{"url":"https://..."}` triggers OTA.
- **Last Will** on `alive` so a hard disconnect publishes `{"status":"offline"}` automatically.
- **Secrets via Kconfig** (`firmware/main/Kconfig.projbuild`). WiFi + MQTT credentials live in `sdkconfig` (gitignored), set once via `idf.py menuconfig`.

## Hardware

- 1× **Freenove FNK0090** ESP32-WROOM-32 dev board (4 MB flash, 240 MHz dual core, onboard LED on GPIO2). Any ESP32 dev board with ≥4 MB flash works; you may need to change the LED pin in `firmware/main/main.c`.
- USB cable + a Windows host running WSL2 Ubuntu (or any Linux box that can run ESP-IDF v5.3).

## Cloud accounts

- **HiveMQ Cloud** — free tier sufficient. Need cluster URL + an Access Management credential with publish/subscribe on `fleet/#`.
- **Cloudflare R2** — free tier sufficient (10 GB / 1M Class A ops). Bucket with the R2.dev public-development URL enabled. Payment method on file required even for the free tier.

## Setup (clone-and-run)

Inside WSL Ubuntu (or any Linux):

```bash
# 1. ESP-IDF v5.3 toolchain (one-time, ~2 GB)
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
sudo usermod -aG dialout $USER  # log out + in afterwards
git clone -b release/v5.3 --recursive --depth 1 --shallow-submodules \
    https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32

# 2. Clone this repo
git clone https://github.com/<you>/fleetos.git
cd fleetos

# 3. Set device-side secrets (WiFi + HiveMQ creds)
./tools/build.sh menuconfig
#   → FleetOS configuration → set 5 string fields → S, Q

# 4. Set host-side secrets
cp host/.env.example host/.env
#   → edit host/.env with HiveMQ HOST/PORT/USERNAME/PASSWORD

# 5. Build + flash + watch
./tools/build.sh flash
python3 -m venv ~/fleetos-venv
source ~/fleetos-venv/bin/activate
pip install -r host/requirements.txt
python host/fleetctl.py status
```

On Windows, plug the board in then bridge it into WSL with [usbipd-win](https://github.com/dorssel/usbipd-win):

```powershell
# admin PowerShell, one-time per board
usbipd bind --busid <X-Y>
# every WSL session
usbipd attach --wsl --busid <X-Y>
```

## Layout

```
firmware/         ESP-IDF project (runs on the ESP32)
  main/             entry, wifi, mqtt, ota modules
  partitions.csv    two-slot OTA layout
  sdkconfig.defaults  shareable build config (4MB flash, CA bundle, rollback)
host/             Python CLI (runs on your laptop)
  fleetctl.py       status / cmd subcommands
tools/            build + release helpers
firmware-builds/  staged .bin artifacts ready for R2 upload (gitignored)
BUILD_LOG.md      day-by-day journal of what worked, what broke, what I learned
CLAUDE.md         scope rules + working agreements
```

## Status

5-day proof-of-concept, currently at end-of-Day-3 (full pipeline live, tagged `v2.0.0` for the demo). Day 4 was the prototype-quality pass that landed rollback safety, the status topic, the heartbeat, and the release helper (tagged `v3.0.0`). Day 5 is the demo video + the LinkedIn post.

See [BUILD_LOG.md](BUILD_LOG.md) for the daily log — what worked, what broke, every gotcha hit along the way.

## What's deliberately out of scope

This is a 5-day POC for outreach, not a product. The following are intentional non-goals:

- Web dashboard — the host CLI is the fleet manager.
- User accounts / auth / RBAC / multi-tenancy.
- Database. Device state lives in MQTT retained messages.
- Custom MQTT broker. HiveMQ Cloud only.
- A/B rollout, canary, staged deployment. One device, one update.
- Encrypted firmware, signed firmware, secure boot — would be Day 1 of a real product but not the story we're telling here.
- Bluetooth, mesh, LoRa, ESP-NOW. WiFi only.
- Arduino fallback. The C-on-ESP-IDF path is the deliberate choice.
