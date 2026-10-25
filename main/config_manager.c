#include "config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "CONFIG";

#define NVS_NAMESPACE   "watch_cfg"

#define DEFAULT_WIFI_SSID       ""
#define DEFAULT_WIFI_PASSWORD   ""
#define DEFAULT_DEEPSEEK_KEY    ""
#define DEFAULT_BRIGHTNESS      80
#define DEFAULT_TIMEZONE        3
#define DEFAULT_WIFI_ENABLED    true

static app_config_t s_config = {0};

esp_err_t config_load(void)
{
    memset(&s_config, 0, sizeof(s_config));
    strncpy(s_config.wifi_ssid,     DEFAULT_WIFI_SSID,     CONFIG_STR_MAX - 1);
    strncpy(s_config.wifi_password, DEFAULT_WIFI_PASSWORD, CONFIG_STR_MAX - 1);
    strncpy(s_config.deepseek_key,  DEFAULT_DEEPSEEK_KEY,  CONFIG_STR_MAX - 1);
    s_config.brightness      = DEFAULT_BRIGHTNESS;
    s_config.timezone_offset = DEFAULT_TIMEZONE;
    s_config.wifi_enabled    = DEFAULT_WIFI_ENABLED;
    s_config.valid           = false;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS пусто, используем значения по умолчанию");
        return ESP_OK;
    }

    size_t len;

    len = CONFIG_STR_MAX;
    nvs_get_str(handle, "wifi_ssid", s_config.wifi_ssid, &len);

    len = CONFIG_STR_MAX;
    nvs_get_str(handle, "wifi_pass", s_config.wifi_password, &len);

    len = CONFIG_STR_MAX;
    nvs_get_str(handle, "ds_key", s_config.deepseek_key, &len);

    nvs_get_u8(handle, "brightness", &s_config.brightness);
    nvs_get_i8(handle, "tz_offset", &s_config.timezone_offset);

    uint8_t wifi_en = 1;
    if (nvs_get_u8(handle, "wifi_en", &wifi_en) == ESP_OK) {
        s_config.wifi_enabled = (wifi_en != 0);
    }

    nvs_close(handle);

    s_config.valid = true;
    ESP_LOGI(TAG, "Конфиг загружен: SSID='%s', brightness=%d, tz=%d, wifi=%s",
             s_config.wifi_ssid, s_config.brightness, s_config.timezone_offset,
             s_config.wifi_enabled ? "ON" : "OFF");
    return ESP_OK;
}

const app_config_t *config_get(void)
{
    return &s_config;
}

static esp_err_t save_str(nvs_handle_t h, const char *key, const char *val)
{
    return nvs_set_str(h, key, val ? val : "");
}

esp_err_t config_set_wifi(const char *ssid, const char *password)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    save_str(h, "wifi_ssid", ssid);
    save_str(h, "wifi_pass", password);
    nvs_commit(h);
    nvs_close(h);

    strncpy(s_config.wifi_ssid,     ssid ? ssid : "",         CONFIG_STR_MAX - 1);
    strncpy(s_config.wifi_password, password ? password : "", CONFIG_STR_MAX - 1);
    s_config.valid = true;

    ESP_LOGI(TAG, "Wi-Fi сохранён: SSID='%s'", s_config.wifi_ssid);
    return ESP_OK;
}

esp_err_t config_set_deepseek_key(const char *key)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    save_str(h, "ds_key", key);
    nvs_commit(h);
    nvs_close(h);

    strncpy(s_config.deepseek_key, key ? key : "", CONFIG_STR_MAX - 1);
    ESP_LOGI(TAG, "API-ключ сохранён");
    return ESP_OK;
}

esp_err_t config_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_u8(h, "brightness", brightness);
    nvs_commit(h);
    nvs_close(h);

    s_config.brightness = brightness;
    ESP_LOGI(TAG, "Яркость сохранена: %d%%", brightness);
    return ESP_OK;
}

esp_err_t config_set_timezone(int8_t offset)
{
    if (offset < -12) offset = -12;
    if (offset > 14)  offset = 14;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_i8(h, "tz_offset", offset);
    nvs_commit(h);
    nvs_close(h);

    s_config.timezone_offset = offset;
    ESP_LOGI(TAG, "Часовой пояс сохранён: UTC%+d", offset);
    return ESP_OK;
}

esp_err_t config_set_wifi_enabled(bool enabled)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_u8(h, "wifi_en", enabled ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);

    s_config.wifi_enabled = enabled;
    ESP_LOGI(TAG, "Wi-Fi %s", enabled ? "включён" : "выключен");
    return ESP_OK;
}

esp_err_t config_reset(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);

    memset(&s_config, 0, sizeof(s_config));
    s_config.brightness = DEFAULT_BRIGHTNESS;
    s_config.timezone_offset = DEFAULT_TIMEZONE;
    s_config.wifi_enabled = DEFAULT_WIFI_ENABLED;

    ESP_LOGW(TAG, "Конфиг сброшен к заводским настройкам");
    return ESP_OK;
}
