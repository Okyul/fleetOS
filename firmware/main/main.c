// FleetOS firmware — Day 1 build.
// Goal: blink the onboard LED at 1Hz (500ms on, 500ms off) and log a version
// string at boot. No WiFi, no MQTT, no OTA yet — we add those Days 2 and 3.
//
// FIRMWARE_VERSION is the single value we'll change between v1 and v2 to prove
// the OTA pipeline is doing real work. v1 = 1Hz blink, v2 = 5Hz blink.

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define FIRMWARE_VERSION    "1.0.0"

// GPIO2 is the conventional onboard-LED pin on most ESP32 dev boards including
// the Freenove FNK0090. If your board's LED is on a different pin (some
// Freenove revisions use GPIO13 or wire it externally), change this and
// reflash. Easiest check: view the schematic in the Freenove tutorial PDF.
#define BLINK_GPIO          GPIO_NUM_2

// Half-period in ms. 500ms on + 500ms off = 1Hz. v2 will use 100ms = 5Hz.
#define BLINK_HALF_PERIOD_MS 500

static const char *TAG = "fleetos";

void app_main(void)
{
    ESP_LOGI(TAG, "FleetOS firmware v%s booting", FIRMWARE_VERSION);

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    bool level = false;
    while (true) {
        level = !level;
        gpio_set_level(BLINK_GPIO, level);
        vTaskDelay(pdMS_TO_TICKS(BLINK_HALF_PERIOD_MS));
    }
}
