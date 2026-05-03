# FleetOS POC — Build Log

Goal: 5-day POC producing a 60–90 second demo video of an ESP32 over-the-air firmware update. Used for outreach to potential customers.

Hardware: Freenove ESP32 Dev Board (FNK0090) — ESP32-WROOM-32, 4MB flash, 240MHz dual-core, onboard WiFi.
Toolchain: ESP-IDF v5.3 inside WSL2 (Ubuntu) on Windows 11.
Cloud: Cloudflare R2 (firmware hosting), HiveMQ Cloud (MQTT broker).

---

## Day 1 — Setup + first boot

**Goal:** ESP-IDF installed, hardware confirmed working, LED blinking at 1Hz from custom code.

### What we did
- _(fill in as we go)_

### What worked
- _tbd_

### What broke
- _tbd_

### What I learned
- _tbd_

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
