#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "wifi_connect.h"

static const char *TAG = "wifi_connect";
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECT_FAILED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECT_CONNECTED_BIT);
    }
}

esp_err_t wifi_connect_start(const wifi_connect_config_t *config,
                             EventGroupHandle_t *event_group)
{
    if (config == NULL || config->sta_config == NULL || event_group == NULL ||
        config->attempts <= 0 || config->timeout_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    esp_netif_create_default_wifi_sta();
    if (config->enable_ap) {
        if (config->ap_config == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&wifi_init_config);
    if (result != ESP_OK && result != ESP_ERR_WIFI_INIT_STATE) {
        return result;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_mode_t mode = config->enable_ap ? WIFI_MODE_APSTA : WIFI_MODE_STA;
    wifi_config_t sta_config = *config->sta_config;
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    if (config->enable_ap) {
        wifi_config_t ap_config = *config->ap_config;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    }
    ESP_ERROR_CHECK(esp_wifi_start());

    if (config->enable_ap) {
        ESP_LOGI(TAG, "Local AP started");
    }

    for (int attempt = 1; attempt <= config->attempts; ++attempt) {
        xEventGroupClearBits(s_wifi_event_group,
                             WIFI_CONNECT_CONNECTED_BIT | WIFI_CONNECT_FAILED_BIT);
        ESP_LOGI(TAG, "Connecting to %s (attempt %d/%d)",
                 config->connection_name == NULL ? "Wi-Fi" : config->connection_name,
                 attempt, config->attempts);

        result = esp_wifi_connect();
        if (result != ESP_OK && result != ESP_ERR_WIFI_CONN) {
            ESP_LOGW(TAG, "Wi-Fi connect request failed: %s", esp_err_to_name(result));
        }

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group, WIFI_CONNECT_CONNECTED_BIT,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(config->timeout_ms));
        if (bits & WIFI_CONNECT_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to %s",
                     config->connection_name == NULL ? "Wi-Fi" : config->connection_name);
            *event_group = s_wifi_event_group;
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Wi-Fi attempt %d failed or timed out", attempt);
    }

    ESP_LOGW(TAG, "%s unavailable after %d attempts",
             config->connection_name == NULL ? "Wi-Fi" : config->connection_name,
             config->attempts);
    *event_group = s_wifi_event_group;
    return ESP_OK;
}
