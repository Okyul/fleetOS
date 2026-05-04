# FleetOS POC — Build Log

Goal: 5-day POC producing a 60–90 second demo video of an ESP32 over-the-air firmware update. Used for outreach to potential customers.

Hardware: Freenove ESP32 Dev Board (FNK0090) — ESP32-WROOM-32, 4MB flash, 240MHz dual-core, onboard WiFi.
Toolchain: ESP-IDF v5.3 inside WSL2 (Ubuntu) on Windows 11.
Cloud: Cloudflare R2 (firmware hosting), HiveMQ Cloud (MQTT broker).

---

## Day 1 — Setup + first boot

**Goal:** ESP-IDF installed, hardware confirmed working, LED blinking at 1Hz from custom code.

**Status:** Day 1 complete. ✅ Toolchain installed, ✅ board flashed, ✅ 1Hz blink visually confirmed, tagged `v1.0.0`.

### What we did
- Installed WSL2 + Ubuntu 24.04 (`wsl --install -d Ubuntu`).
- Installed usbipd-win 5.3.0 (`winget install --id dorssel.usbipd-win`).
- `git init` + first commit of the scaffold (CLAUDE.md, README.md, BUILD_LOG.md, .gitignore, firmware/ skeleton).
- Apt prereqs for ESP-IDF: `git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0`.
- Added `connor` to `dialout` group (needed for serial without sudo when board arrives).
- Cloned ESP-IDF release/v5.3 (shallow + shallow submodules) to `~/esp/esp-idf` — 599 MB.
- Ran `install.sh esp32`: Xtensa GCC 13.2.0, Python 3.12 venv with esptool 4.11, all deps in `~/.espressif/`.
- Built `firmware/main.c` end-to-end: `fleetos.bin` 163 KB (84% of partition free).
- Added `tools/build.sh` to absorb the path-workaround.
- **Day 1b (board arrived):** `usbipd bind 2-1` (admin PS, one-time) → `usbipd attach --wsl --busid 2-1`. Board appeared as `/dev/ttyUSB0` (root:dialout). `./tools/build.sh flash` wrote bootloader + partition-table + app at 460 kbaud, hard-reset via RTS pin. Captured boot log over serial via a tiny Python DTR/RTS pulse + read script. Boot log shows `I (310) fleetos: FleetOS firmware v1.0.0 booting` + `GPIO[2]` configured. Visual confirmation: onboard LED blinking at 1Hz. Tagged `v1.0.0`.

### What worked
- ESP-IDF install was clean — single `install.sh esp32` call, no manual fiddling.
- `idf.py set-target esp32` configured fine even at the Dropbox path.
- Once mirrored to `~/fleetos-poc/`, the build succeeded on the first try and produced a valid esp32 image.
- Hardware came up first try. CH340 driver was already installed on Windows (board enumerated as COM5). usbipd handed it cleanly to WSL. esptool flashed at 460 kbaud, no retries.
- `ESP_LOGI(TAG, ...)` survives unchanged from source to serial output — easy to grep.

### What broke
- **Parens in path break Ninja.** The Dropbox path `My PC (LAPTOP-M5075A75)` made the bootloader `check_sizes` step fail with `/bin/sh: 1: Syntax error: "(" unexpected`. Some ESP-IDF custom commands escape spaces with backslashes but not parens, so `/bin/sh` choked on the literal `(`. Fix: keep source in Dropbox, build at `~/fleetos-poc/firmware/`. `tools/build.sh` rsyncs before building.
- WSL passwordless sudo isn't on by default — bundled all sudo work into one paste so the password prompt only happened once.
- Git Bash on Windows mangles absolute Linux paths when calling `wsl -- bash -c "/mnt/c/..."` — it prepends `C:/Program Files/Git`. Workaround: invoke with `wsl -- bash -lc 'cd /mnt/c/... && ...'` from a session, or just run `tools/build.sh` from inside a WSL terminal directly.

### What I learned
- ESP-IDF v5.3 install is one script and ~600 MB cloned + ~1 GB of toolchain — fast on decent connection.
- The build system produces three artifacts: `bootloader.bin`, `partition-table.bin`, `fleetos.bin`. All three flash separately at known offsets; `idf.py flash` chains them with the right offsets.
- `app_main()` is the entry point ESP-IDF expects in the `main` component. FreeRTOS is already running by the time it's called.
- Partition usage is reported in the build output: 84% free in the smallest app slot today, will shrink once WiFi + MQTT + OTA libraries link in on Days 2–3.
- **Flash size mismatch (Day 3 todo):** boot log warned `Detected size(4096k) larger than the size in the binary image header(2048k)`. Board has 4 MB, sdkconfig defaults to 2 MB. Day 3 has to bump `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` to fit two OTA app slots.
- **DTR/RTS reset trick:** to capture the boot log non-interactively without `idf.py monitor`, pulse RTS while DTR is low — that's what esptool does internally for `--after hard_reset`. Useful pattern for scripted log capture later.
- usbipd `bind` is the admin-only one-time step; `attach` is per-session and doesn't need admin once bound.

### Time spent
- ~45 min for Day 1a (setup + first build). ~10 min for Day 1b (flash + visual confirm) once the board arrived.

### Day 2 first action
1. Add `nvs_flash`, `esp_wifi`, `esp_event`, `esp_netif` to `firmware/main/CMakeLists.txt` REQUIRES.
2. WiFi station init in `app_main`, connect to home network. Creds need to NOT be in git — use `sdkconfig.defaults` with placeholders + a `secrets.h` (gitignored) for real values, or push them through Kconfig prompts. Decide before writing code.
3. Add MQTT client (`esp-mqtt`) connecting to HiveMQ Cloud over TLS. HiveMQ gives a `mqtts://...:8883` URL + username/password — load from the same secrets path as WiFi.
4. On connect, publish `fleet/<device-id>/alive` with JSON `{"version":"1.0.0","mac":"ec:e3:34:a4:0d:4c"}`. Subscribe to `fleet/<device-id>/cmd`.
5. Python host CLI: `host/fleetctl.py status` subscribes to `fleet/+/alive`, prints what it sees.

---

## Day 2 — WiFi + MQTT

**Goal:** Device connects to WiFi, connects to HiveMQ Cloud over TLS, subscribes to a topic, publishes "I'm alive" with firmware version on boot. Python host script can send/receive on the same broker.

---

## Day 3 — OTA flow

**Goal:** Custom partition table, `esp_https_ota` working end-to-end. Laptop publishes URL → device downloads from R2 → device flashes → device reboots into new firmware → version reports back.

---

## Day 4 — Hardening + the video

**Goal:** Failure modes handled (bad URL, network drop, version mismatch). Logging that looks impressive on video. Demo recorded.

---

## Day 5 — Polish + ship

**Goal:** README written. LinkedIn post drafted. Pushed to public GitHub.
