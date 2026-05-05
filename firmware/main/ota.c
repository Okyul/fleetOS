// HTTPS OTA driver + rollback safety.
//
// OTA flow: fleetos_ota_start(url) copies url, spawns ota_task, returns.
// ota_task runs esp_https_ota (blocking) with the system CA bundle, then
// esp_restart on success.
//
// Rollback safety (when CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y, which
// sdkconfig.defaults sets): the bootloader marks freshly-OTA'd images as
// PENDING_VERIFY on first boot. fleetos_ota_init checks that state. If
// pending, it spawns a watchdog that reboots after 60s if mark_valid hasn't
// been called — bootloader then reverts to the previous good slot.

#include "ota.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"

#include "mqtt.h"

static const char *TAG = "ota";

#define OTA_URL_MAX 1024
#define ROLLBACK_TIMEOUT_MS 60000

typedef struct {
    char url[OTA_URL_MAX];
} ota_args_t;

// Set true once we've confirmed the app is healthy. Read by the rollback
// watchdog task. Plain bool is fine — single writer, single reader, never
// transitions back to false.
static volatile bool s_marked_valid;

static void ota_task(void *pv)
{
    ota_args_t *args = (ota_args_t *)pv;
    ESP_LOGI(TAG, "starting OTA from %s", args->url);
    fleetos_mqtt_publish_status("downloading", args->url);

    esp_http_client_config_t http_cfg = {
        .url = args->url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    free(args);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA succeeded, rebooting in 2s");
        fleetos_mqtt_publish_status("rebooting", NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    fleetos_mqtt_publish_status("error", esp_err_to_name(ret));
    vTaskDelete(NULL);
}

static void rollback_watchdog_task(void *pv)
{
    ESP_LOGI(TAG, "rollback watchdog armed: must mark valid within %d ms",
             ROLLBACK_TIMEOUT_MS);
    vTaskDelay(pdMS_TO_TICKS(ROLLBACK_TIMEOUT_MS));

    if (s_marked_valid) {
        ESP_LOGI(TAG, "watchdog: app marked valid, exiting");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGE(TAG, "watchdog: app NOT marked valid in %d ms; rolling back",
             ROLLBACK_TIMEOUT_MS);
    // This call writes INVALID to otadata for the running slot and reboots.
    // The bootloader then picks the previous slot.
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // Unreachable.
}

void fleetos_ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGW(TAG, "no running partition info; skipping rollback setup");
        return;
    }

    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        ESP_LOGW(TAG, "could not read partition state; skipping rollback setup");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "running PENDING_VERIFY image (slot %s); arming rollback watchdog",
                 running->label);
        xTaskCreate(rollback_watchdog_task, "ota_wd", 3072, NULL, 5, NULL);
    } else {
        // Either ESP_OTA_IMG_VALID, ESP_OTA_IMG_UNDEFINED (first flash via
        // idf.py flash, no OTA yet), or one of the other states. None require
        // the watchdog — there's nothing to roll back to.
        ESP_LOGI(TAG, "running slot %s state=%d; no rollback armed",
                 running->label, (int)state);
        s_marked_valid = true;
    }
}

void fleetos_ota_mark_valid(void)
{
    if (s_marked_valid) {
        return;
    }
    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "marked app valid, rollback cancelled");
        s_marked_valid = true;
    } else {
        ESP_LOGW(TAG, "esp_ota_mark_app_valid_cancel_rollback failed: %s",
                 esp_err_to_name(ret));
    }
}

void fleetos_ota_start(const char *url)
{
    if (!url || strlen(url) == 0) {
        ESP_LOGW(TAG, "fleetos_ota_start called with empty url");
        return;
    }
    if (strncmp(url, "https://", 8) != 0) {
        ESP_LOGW(TAG, "rejecting non-https URL: %s", url);
        return;
    }

    ota_args_t *args = calloc(1, sizeof(*args));
    if (!args) {
        ESP_LOGE(TAG, "OOM allocating ota args");
        return;
    }
    strlcpy(args->url, url, sizeof(args->url));

    BaseType_t r = xTaskCreate(ota_task, "ota", 8192, args, 5, NULL);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "failed to spawn ota task");
        free(args);
    }
}
