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

**Status:** ✅ complete. Device boots → WiFi → MQTT/TLS → publishes retained `{"version":"1.0.0","device_id":"ece334a40d4c","uptime_ms":3878}` on `fleet/<device-id>/alive`. Host CLI receives it on subscribe.

### What we did
- **Secrets via Kconfig.** `firmware/main/Kconfig.projbuild` exposes 5 string options (WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_URI, MQTT_USERNAME, MQTT_PASSWORD). User sets via `idf.py menuconfig`; values bake into `sdkconfig` (gitignored).
- **`sdkconfig.defaults` is now committed** (was previously gitignored as a "secrets dump"). Holds shareable build defaults: `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`. Updated `.gitignore` comment to match.
- **Split firmware into modules:** `wifi.c/h` (STA bring-up, event handling, MAC→device-id), `mqtt.c/h` (TLS connect, retained alive publish, cmd subscribe). `main.c` orchestrates: NVS init → blink task → wifi (blocking) → mqtt (non-blocking).
- **TLS via crt_bundle.** `esp_crt_bundle_attach` uses ESP-IDF's bundled CA store. No pinned cert to maintain; HiveMQ can rotate Let's Encrypt certs without breaking us.
- **Last Will** registered on the alive topic so a hard disconnect publishes `{"status":"offline"}` to the same retained topic.
- **Host CLI:** `host/fleetctl.py` with `status` (subscribe to `fleet/+/alive`) and `cmd` (publish to `fleet/<id>/cmd`) subcommands. Credentials via `.env` (gitignored) + python-dotenv. `host/.env.example` checked in as template.

### What worked
- Kconfig prompt UX is fine once you know `Enter` opens the input dialog, paste with `Ctrl+Shift+V`, `Enter` to commit, `S` then `Q` to save+quit.
- crt_bundle "just worked" against HiveMQ Cloud's Let's Encrypt cert — no pinning, no PEM file, no hassle.
- Last Will + retained alive on the same topic gives the host CLI a single-topic-per-device view of liveness for free.
- `paho-mqtt` 2.x `tls_set()` with no args picks up Ubuntu's system CA bundle correctly. No `certifi` needed.

### What broke
- **Menuconfig silently dropped 3 chars from the broker URI on paste** (`bd6` missing from the cluster ID). The truncated hostname still resolved into HiveMQ's shared infrastructure and TLS handshake succeeded against a wildcard cert, but the wrong cluster rejected with `"Connection refused, not authorized"` (`type=2`, `tls=0x0`). Fix: `sed -i` the correct URI directly into `~/fleetos-poc/firmware/sdkconfig`. Lesson: for long URIs, paste then visually verify the field, or `grep MQTT_BROKER_URI ~/fleetos-poc/firmware/sdkconfig` after exiting menuconfig.
- **Ubuntu 24.04 PEP 668 blocks `pip install` system-wide.** Need a venv. Bonus: a venv created at `host/.venv` (on `/mnt/c`) inherited the externally-managed marker because Linux symlinks don't fully resolve on Windows DrvFS. Fix: put venvs at `~/fleetos-venv` (Linux home), not `/mnt/c`.
- **Stale interactive WSL session swallowed Python output.** First two attempts to run `python -u fleetctl.py status` produced zero terminal output despite the script working perfectly when invoked from a non-interactive `wsl -- bash -lc` shell. `wsl --shutdown` + fresh terminal fixed it. No clean root-cause; suspect a console-host stdout-buffer wedge, possibly from an earlier broken venv.

### What I learned
- esp-mqtt event API: register `ESP_EVENT_ANY_ID`, dispatch on `event_id` inside the handler. `esp_mqtt_client_publish` returns the msg_id (useful for log correlation).
- `esp_event_handler_instance_register` for both `WIFI_EVENT, ESP_EVENT_ANY_ID` and `IP_EVENT, IP_EVENT_STA_GOT_IP`. Same handler can dispatch on `(base, id)`.
- Retained + QoS 1 on a single per-device topic is the lightest possible "is this device alive?" pattern. No separate keepalive topic, no metric DB.
- Binary grew from 163 KB (Day 1) to 962 KB (Day 2) — WiFi + TLS + cJSON + MQTT all linked in. Smallest factory app partition is 1 MB at default partition table; we're at **8% free**. Day 3's custom partition table (two 1 MB OTA slots) will need careful sizing — may need to bump to 1.25 MB slots or strip log strings.

### Time spent
- ~75 min: write code, ~30 min debugging the menuconfig URI truncation + Python venv issues.

### Day 3 first action
1. Custom partition table at `firmware/partitions.csv` with `nvs`, `phy_init`, `otadata`, `ota_0`, `ota_1` (no `factory`).
2. Verify two 1+ MB app slots fit in 4 MB after bootloader + nvs overhead. If 1 MB is too tight (we were at 8% free), bump to 1.25 MB and shrink `nvs` accordingly.
3. Add `esp_https_ota` call in `mqtt.c` MQTT_EVENT_DATA handler — when topic is `cmd` and JSON has a `url` field, kick off the download.
4. Build v1.0.0 (1Hz) → upload to R2 with public read → push URL via `python fleetctl.py cmd ece334a40d4c '{"url":"https://..."}'` → device flashes → reboots into v1.0.0 again (no-op test).
5. Bump `FIRMWARE_VERSION` to 2.0.0 + change `BLINK_HALF_PERIOD_MS` to 100ms (5Hz) → upload v2 → push URL → blink should change rate after reboot. **That's the demo.**

---

## Day 3 — OTA flow

**Goal:** Custom partition table, `esp_https_ota` working end-to-end. Laptop publishes URL → device downloads from R2 → device flashes → device reboots into new firmware → version reports back.

**Status:** ✅ complete and demo-ready. End-to-end pipeline runs: `python fleetctl.py cmd <id> '{"url":"https://...r2.dev/..."}'` → device downloads → flashes inactive slot → reboots → reports new version. Blink rate visibly changed from 1Hz to 5Hz on the v1→v2 OTA, tagged `v2.0.0`.

### What we did
- **Custom partition table** (`firmware/partitions.csv`): two 1.5 MB OTA app slots (ota_0 at 0x20000, ota_1 at 0x1A0000), no factory. otadata at 0xf000 picks the active slot.
- **`firmware/main/ota.c`**: `fleetos_ota_start(url)` spawns a one-shot FreeRTOS task that calls `esp_https_ota` with `esp_crt_bundle_attach`, then `esp_restart()`. URL is copied into a heap buffer so the caller's stack-borrow doesn't matter; rejected non-HTTPS URLs upfront.
- **MQTT cmd dispatch**: `mqtt.c` MQTT_EVENT_DATA parses payload as JSON via `cJSON_ParseWithLength`, extracts `url`, calls `fleetos_ota_start`. Strict topic match against `s_topic_cmd`.
- **R2 setup**: created bucket `fleetos-firmware`, enabled R2.dev public-development URL, uploaded `fleetos-v1.0.0.bin` and `fleetos-v2.0.0.bin`. Public URLs format: `https://pub-<hash>.r2.dev/<filename>`.
- **`firmware-builds/`** directory created (gitignored) for tagged binary artifacts staged for upload.
- **Demo flip**: bumped `FIRMWARE_VERSION` to `2.0.0` and `BLINK_HALF_PERIOD_MS` to 100 (5Hz). Built v2.0.0, uploaded to R2, pushed URL via cmd, watched blink rate change live.

### What worked
- **Pipeline first try.** Once partition table + OTA module landed, the v1→v1 self-OTA succeeded on first push. Boot log confirmed `Loaded app from partition at offset 0x1a0000` (ota_1) after reboot — exactly the expected slot flip.
- **R2 + esp_crt_bundle.** No cert pinning needed. Cloudflare's `pub-*.r2.dev` certs validated cleanly against the bundled CA store.
- **`esp_https_ota` in a dedicated task** kept the MQTT task free, so keepalives stayed alive during the 1MB download. No mid-OTA broker disconnect.
- **Last Will + retained alive** worked through the reboot: when the device crashed/rebooted mid-OTA the broker would have published the offline payload, but successful OTA → fresh `alive` overwrote it on reconnect with the new version. Single topic, full liveness signal.

### What broke
- **`sdkconfig.defaults` doesn't override existing `sdkconfig`.** Adding `CONFIG_PARTITION_TABLE_CUSTOM=y` to defaults didn't take effect because Day 2's menuconfig run had already baked the single-app partition value into `sdkconfig`. Fix: sed-patched the existing `sdkconfig` directly. Lesson: defaults only fill in *missing* keys; switching an existing setting requires either menuconfig, sed, or wiping sdkconfig (which would lose secrets).
- **Background serial capture came back empty** during the v2 OTA window. Suspect USB device contention — the `usbipd` attached `/dev/ttyUSB0` doesn't tolerate two readers cleanly. Worked around by reading the broker's retained alive instead, which proved the OTA succeeded just as definitively (version field flipped to 2.0.0 with fresh uptime).
- **Bash variable expansion through `wsl -- bash -c "..."`** got mangled when the outer shell pre-expanded `$VAR`. Workaround: inline literal paths, or use `bash -c '...'` (single quotes) when the script body has no need to interpolate parent-shell variables.

### What I learned
- **OTA partition layout in ESP-IDF**: bootloader looks at `otadata` (8 KB) for the "active slot" pointer. On first boot with a fresh `otadata`, it falls through to `ota_0`. After a successful `esp_https_ota`, ESP-IDF writes the new app to the *inactive* slot, then atomically updates `otadata` to point at it. Reboot → new slot active. If the new slot fails to mark itself valid (with rollback enabled), bootloader can revert.
- **Two 1.5 MB slots fit comfortably in 4 MB.** Bootloader + nvs + otadata + phy_init = ~80 KB. 4 MB - 0x20000 (128 KB header offset) = 3.875 MB. Two 1.5 MB slots = 3 MB. Headroom: ~875 KB.
- **`cJSON_ParseWithLength`** is the right call when you have a non-NUL-terminated buffer (like an MQTT payload). Don't use `cJSON_Parse` — it'll happily walk past the buffer end if the payload happens not to terminate with a `}`.
- The build's `Smallest app partition` line is the canonical way to verify your custom partition table actually loaded — if it still says 0x100000 after editing the table, the change didn't apply.

### Time spent
- ~90 min: code, R2 setup, two OTA cycles, debugging the sdkconfig override issue.

### Day 4 first action
1. **Hardening pass** on the OTA failure modes: bad URL (404 from R2), wrong-arch binary (e.g. esp32-s3 image), mid-download network drop, payload that's not JSON.
2. Enable **`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`** + add `esp_ota_mark_app_valid_cancel_rollback()` after a successful boot + first MQTT publish. This way a borked v2 that fails to connect gets auto-reverted by the bootloader on next boot — the "we don't brick devices" part of the story.
3. **Status topic**: publish `fleet/<id>/status` JSON during OTA progress (`downloading`, `applying`, `rebooting`). Day 4 polish that makes the demo video logs look intentional.
4. **Record the video.** Plan the shot list: 1Hz blink visible → run `fleetctl status` (alive shown) → run `fleetctl cmd ... v2.0.0.bin` → watch alive flip from 1.0.0 to 2.0.0 + blink rate change. Aim 60–90s.

---

## Day 4 — Hardening + the video

**Goal:** Failure modes handled (bad URL, network drop, version mismatch). Logging that looks impressive on video. Demo recorded.

---

## Day 5 — Polish + ship

**Goal:** README written. LinkedIn post drafted. Pushed to public GitHub.
