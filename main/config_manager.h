#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define CONFIG_STR_MAX  128

typedef struct {
    char     wifi_ssid[CONFIG_STR_MAX];
    char     wifi_password[CONFIG_STR_MAX];
    char     deepseek_key[CONFIG_STR_MAX];
    uint8_t  brightness;
    int8_t   timezone_offset;
    bool     wifi_enabled;     // Wi-Fi включён?
    bool     valid;
} app_config_t;

esp_err_t config_load(void);
const app_config_t *config_get(void);
esp_err_t config_set_wifi(const char *ssid, const char *password);
esp_err_t config_set_deepseek_key(const char *key);
esp_err_t config_set_brightness(uint8_t brightness);
esp_err_t config_set_timezone(int8_t offset);
esp_err_t config_set_wifi_enabled(bool enabled);
esp_err_t config_reset(void);
