// FleetOS firmware entry point.
//
// Boot order:
//   1. NVS init (esp_wifi requires it).
//   2. fleetos_ota_init — checks the running partition's verification state.
//      If this boot is a freshly-OTA'd image (PENDING_VERIFY), arms a 60s
//      watchdog: if MQTT doesn't connect + publish before then, the device
//      reboots and the bootloader rolls back to the previous good slot.
//   3. Spawn the blink task — visible heartbeat for the demo.
//   4. WiFi station bring-up. Blocks until we have an IPv4 address.
//   5. MQTT client start. Non-blocking; reconnects automatically. On the
//      first successful CONNECTED event, mqtt.c calls fleetos_ota_mark_valid
//      to cancel the rollback watchdog above.
//   6. Heartbeat task — republishes alive every 30s with current uptime,
//      free heap, and RSSI. Lets the host show liveness without waiting
//      for a boot.

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "mqtt.h"
#include "ota.h"

#define FIRMWARE_VERSION    "3.0.2"

// GPIO2 is the FNK0090's onboard LED (active-high). Confirmed against
// Freenove's docs at docs.freenove.com/projects/fnk0090.
#define BLINK_GPIO          GPIO_NUM_2

// Half-period in ms. v1 = 500 (1Hz), v2 = 100 (5Hz), v3 = 250 (2Hz).
// Single source of truth for the visible OTA flip — change this and
// FIRMWARE_VERSION together.
#define BLINK_HALF_PERIOD_MS 100

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

    // Must run before MQTT — arms the rollback watchdog if we just came up
    // from an OTA, so that if anything below fails we get reverted.
    fleetos_ota_init();

    xTaskCreate(blink_task, "blink", 2048, NULL, 5, NULL);

    fleetos_wifi_start();

    static char device_id[13];
    fleetos_wifi_mac_id(device_id, sizeof(device_id));
    ESP_LOGI(TAG, "device_id=%s", device_id);

    fleetos_mqtt_start(device_id, FIRMWARE_VERSION);
    fleetos_mqtt_heartbeat_start();

    ESP_LOGI(TAG, "boot complete, app_main returning");
    // app_main returns; FreeRTOS keeps the blink, mqtt, and heartbeat tasks
    // (plus the rollback watchdog if armed) running.
}
