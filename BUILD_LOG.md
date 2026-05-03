# FleetOS POC — Build Log

Goal: 5-day POC producing a 60–90 second demo video of an ESP32 over-the-air firmware update. Used for outreach to potential customers.

Hardware: Freenove ESP32 Dev Board (FNK0090) — ESP32-WROOM-32, 4MB flash, 240MHz dual-core, onboard WiFi.
Toolchain: ESP-IDF v5.3 inside WSL2 (Ubuntu) on Windows 11.
Cloud: Cloudflare R2 (firmware hosting), HiveMQ Cloud (MQTT broker).

---

## Day 1 — Setup + first boot

**Goal:** ESP-IDF installed, hardware confirmed working, LED blinking at 1Hz from custom code.

**Status:** Day 1a complete (toolchain + build proven). Day 1b (flash + visual blink) deferred — board ships in ~2 days.

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

### What worked
- ESP-IDF install was clean — single `install.sh esp32` call, no manual fiddling.
- `idf.py set-target esp32` configured fine even at the Dropbox path.
- Once mirrored to `~/fleetos-poc/`, the build succeeded on the first try and produced a valid esp32 image.

### What broke
- **Parens in path break Ninja.** The Dropbox path `My PC (LAPTOP-M5075A75)` made the bootloader `check_sizes` step fail with `/bin/sh: 1: Syntax error: "(" unexpected`. Some ESP-IDF custom commands escape spaces with backslashes but not parens, so `/bin/sh` choked on the literal `(`. Fix: keep source in Dropbox, build at `~/fleetos-poc/firmware/`. `tools/build.sh` rsyncs before building.
- WSL passwordless sudo isn't on by default — bundled all sudo work into one paste so the password prompt only happened once.
- Git Bash on Windows mangles absolute Linux paths when calling `wsl -- bash -c "/mnt/c/..."` — it prepends `C:/Program Files/Git`. Workaround: invoke with `wsl -- bash -lc 'cd /mnt/c/... && ...'` from a session, or just run `tools/build.sh` from inside a WSL terminal directly.

### What I learned
- ESP-IDF v5.3 install is one script and ~600 MB cloned + ~1 GB of toolchain — fast on decent connection.
- The build system produces three artifacts: `bootloader.bin`, `partition-table.bin`, `fleetos.bin`. All three flash separately at known offsets; `idf.py flash` chains them with the right offsets.
- `app_main()` is the entry point ESP-IDF expects in the `main` component. FreeRTOS is already running by the time it's called.
- Partition usage is reported in the build output: 84% free in the smallest app slot today, will shrink once WiFi + MQTT + OTA libraries link in on Days 2–3.

### Time spent
- ~45 min wall clock from "files transferred" to "build green".

### Tomorrow's first action
1. **Once the ESP32 arrives:** plug into Windows USB, install CH340 driver if Device Manager doesn't auto-detect (https://www.wch-ic.com/downloads/CH341SER_EXE.html).
2. From admin PowerShell: `usbipd list` → find the CH340/CP210x device → `usbipd bind --busid X-Y` → `usbipd attach --wsl --busid X-Y`. Verify `ls /dev/ttyUSB*` shows up in WSL.
3. From inside a WSL terminal at the project root: `./tools/build.sh flash monitor`. Expect 1Hz blink on GPIO2 + boot log line `FleetOS firmware v1.0.0 booting`.
4. Tag commit as `v1.0.0` once the blink is visually confirmed.
5. Then start Day 2 (WiFi + MQTT).

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
