#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECT_CONNECTED_BIT BIT0
#define WIFI_CONNECT_FAILED_BIT BIT1

typedef struct {
    const char *connection_name;
    const wifi_config_t *sta_config;
    bool enable_ap;
    const wifi_config_t *ap_config;
    int attempts;
    int timeout_ms;
} wifi_connect_config_t;

esp_err_t wifi_connect_start(const wifi_connect_config_t *config,
                             EventGroupHandle_t *event_group);

#endif
