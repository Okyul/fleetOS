// FleetOS firmware entry point.
//
// Boot order:
//   1. NVS init (esp_wifi requires it).
//   2. Spawn the blink task — visible heartbeat for the demo. Day 3 will
//      change BLINK_HALF_PERIOD_MS in v2 firmware to prove OTA actually
//      swapped.
//   3. WiFi station bring-up. Blocks until we have an IPv4 address.
//   4. MQTT client start. Non-blocking — runs forever in its own task,
//      reconnects automatically, and publishes the retained "alive" message
//      whenever it (re)connects.
//
// Day 2 scope: connect, publish alive, subscribe to cmd, log inbound.
// Day 3 will dispatch cmd payloads to OTA. Day 4 hardens failure modes.

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "mqtt.h"

#define FIRMWARE_VERSION    "1.0.0"

// GPIO2 is the FNK0090's onboard LED (active-high). Confirmed against
// Freenove's own docs at docs.freenove.com/projects/fnk0090.
#define BLINK_GPIO          GPIO_NUM_2

// Half-period in ms. 500ms on + 500ms off = 1Hz. v2 firmware will set this
// to 100ms (5Hz) so the OTA flip is unmistakable on video.
#define BLINK_HALF_PERIOD_MS 500

static const char *TAG = "fleetos";

static void blink_task(void *arg)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    bool level = false;
    while (true) {
        level = !level;
        gpio_set_level(BLINK_GPIO, level);
        vTaskDelay(pdMS_TO_TICKS(BLINK_HALF_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FleetOS firmware v%s booting", FIRMWARE_VERSION);

    // NVS holds WiFi calibration plus PMK cache. esp_wifi_init expects it
    // ready. If the partition is corrupt or out of date, erase + re-init.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(blink_task, "blink", 2048, NULL, 5, NULL);

    fleetos_wifi_start();

    static char device_id[13];
    fleetos_wifi_mac_id(device_id, sizeof(device_id));
    ESP_LOGI(TAG, "device_id=%s", device_id);

    fleetos_mqtt_start(device_id, FIRMWARE_VERSION);

    ESP_LOGI(TAG, "boot complete, app_main returning");
    // app_main returns; FreeRTOS keeps the blink task and MQTT task running.
}
