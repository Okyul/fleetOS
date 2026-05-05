// esp-mqtt client connecting to HiveMQ Cloud over TLS.
//
// TLS trust anchors come from ESP-IDF's bundled CA store (enabled in
// sdkconfig.defaults via CONFIG_MBEDTLS_CERTIFICATE_BUNDLE). HiveMQ Cloud's
// certs chain to Let's Encrypt's ISRG Root X1, which is in the bundle, so
// the broker validates without us pinning anything.
//
// Topic structure:
//   fleet/<device-id>/alive   -- retained boot announcement (this client publishes)
//   fleet/<device-id>/cmd     -- inbound commands from host (this client subscribes)
//   fleet/<device-id>/status  -- runtime status updates (Day 3+)

#include "mqtt.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_crt_bundle.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "sdkconfig.h"

static const char *TAG = "mqtt";

// Borrowed at start time; freed never (intentional — they're meant to be
// `static const` from main.c).
static const char *s_device_id;
static const char *s_firmware_version;

static char s_topic_alive[64];
static char s_topic_cmd[64];

// Build the alive JSON payload. Caller frees with cJSON_free.
static char *build_alive_payload(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", s_firmware_version);
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    // esp_timer_get_time returns microseconds since boot.
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static void event_handler(void *handler_args, esp_event_base_t base,
                          int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "connected to broker");

        char *payload = build_alive_payload();
        // QoS 1 + retained — anyone subscribing later sees the latest alive.
        int msg_id = esp_mqtt_client_publish(client, s_topic_alive, payload, 0, 1, 1);
        ESP_LOGI(TAG, "published alive on %s (msg_id=%d): %s", s_topic_alive, msg_id, payload);
        cJSON_free(payload);

        msg_id = esp_mqtt_client_subscribe(client, s_topic_cmd, 1);
        ESP_LOGI(TAG, "subscribed %s (msg_id=%d)", s_topic_cmd, msg_id);
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected from broker");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "rx topic=%.*s payload=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        // Day 3 will dispatch this to OTA when topic == cmd and payload has
        // a "url" field. For now, we just log.
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "error: type=%d, tls=0x%x, transport=0x%x",
                 event->error_handle->error_type,
                 event->error_handle->esp_tls_last_esp_err,
                 event->error_handle->esp_transport_sock_errno);
        break;
    default:
        break;
    }
}

void fleetos_mqtt_start(const char *device_id, const char *firmware_version)
{
    s_device_id = device_id;
    s_firmware_version = firmware_version;

    snprintf(s_topic_alive, sizeof(s_topic_alive), "fleet/%s/alive", device_id);
    snprintf(s_topic_cmd, sizeof(s_topic_cmd), "fleet/%s/cmd", device_id);

    // Last Will: if we drop without publishing a clean offline, the broker
    // posts this so the host CLI sees us go down.
    char will_payload[96];
    snprintf(will_payload, sizeof(will_payload),
             "{\"device_id\":\"%s\",\"status\":\"offline\"}", device_id);

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = CONFIG_FLEETOS_MQTT_BROKER_URI,
            .verification.crt_bundle_attach = esp_crt_bundle_attach,
        },
        .credentials = {
            .username = CONFIG_FLEETOS_MQTT_USERNAME,
            .authentication.password = CONFIG_FLEETOS_MQTT_PASSWORD,
            .client_id = device_id,
        },
        .session = {
            .last_will = {
                .topic = s_topic_alive,
                .msg = will_payload,
                .qos = 1,
                .retain = 1,
            },
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "client started, broker=%s", CONFIG_FLEETOS_MQTT_BROKER_URI);
}
