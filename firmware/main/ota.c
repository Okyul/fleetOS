// HTTPS OTA driver. Wraps esp_https_ota in a dedicated FreeRTOS task so the
// MQTT event handler can dispatch and return immediately (esp_https_ota
// blocks for the entire download — calling it from the MQTT task would stall
// keepalives and trigger a broker disconnect mid-update).
//
// Flow:
//   1. fleetos_ota_start(url) — copy url, spawn ota_task, return.
//   2. ota_task — esp_https_ota with the system CA bundle for TLS, then
//      esp_restart on success. On failure, log and exit the task; the device
//      keeps running the current firmware.

#include "ota.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "ota";

// Big enough for an R2 presigned/public URL with a long key. Bumped from 512
// because R2 .r2.dev hostnames + bucket + filename can push past 200 chars
// and presigned URLs add query params.
#define OTA_URL_MAX 1024

typedef struct {
    char url[OTA_URL_MAX];
} ota_args_t;

static void ota_task(void *pv)
{
    ota_args_t *args = (ota_args_t *)pv;
    ESP_LOGI(TAG, "starting OTA from %s", args->url);

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
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    vTaskDelete(NULL);
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

    // 8 KB stack — esp_https_ota + mbedTLS + cJSON path is stack-heavy.
    BaseType_t r = xTaskCreate(ota_task, "ota", 8192, args, 5, NULL);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "failed to spawn ota task");
        free(args);
    }
}
