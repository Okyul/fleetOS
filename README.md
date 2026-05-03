# FleetOS POC

Proof-of-concept over-the-air firmware update pipeline for ESP32. Push a URL via MQTT → device downloads the binary → device flashes itself → device reboots into the new firmware → version reports back.

> Status: in-progress 5-day build. README will be filled in Day 5 with clone-and-run instructions.

## Stack

- **Device:** Freenove ESP32 Dev Board (FNK0090), ESP-IDF v5.3, C
- **Broker:** HiveMQ Cloud (MQTT over TLS)
- **Firmware hosting:** Cloudflare R2
- **Host CLI:** Python 3 + paho-mqtt

## Layout

```
firmware/         ESP-IDF project (runs on the ESP32)
host/             Python CLI (runs on your laptop)
firmware-builds/  Compiled .bin artifacts uploaded to R2 (gitignored)
BUILD_LOG.md      Daily log of what worked, what broke, what I learned
```

See [BUILD_LOG.md](BUILD_LOG.md) for the day-by-day journal.
