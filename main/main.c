#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_wifi_config.h"
#include "esp_bus.h"
#include "rtc_manager.h"
#include "sntp_manager.h"
#include "watch_ui.h"
#include "battery_manager.h"
#include "screen_manager.h"
#include "timer_ui.h"
#include "calculator_ui.h"
#include "speedtest_ui.h"
#include "menu_ui.h"
#include "about_ui.h"
#include "config_manager.h"
#include "settings_ui.h"
#include "settings_key.h"
#include "settings_brightness.h"
#include "settings_timezone.h"
#include "settings_reset.h"
#include "settings_wifi_info.h"
#include "power_manager.h"
#include "web_api.h"

static const char *TAG = "MAIN";

static lv_obj_t *s_watch_screen = NULL;

static void watch_task(void *arg)
{
    rtc_time_t now;
    while (1) {
        if (rtc_manager_get_time(&now) == ESP_OK) {
            bsp_display_lock(0);
            watch_ui_update(&now);
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void sntp_task(void *arg)
{
    ESP_LOGI(TAG, "Ожидание реального подключения к Wi-Fi для SNTP...");
    int waited = 0;
    while (!wifi_cfg_is_connected() && waited < 300) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        waited++;
    }

    if (wifi_cfg_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi подключен! Запуск SNTP-синхронизации...");
        esp_err_t ret = sntp_manager_sync(30);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Время синхронизировано");
        } else {
            ESP_LOGW(TAG, "Не удалось синхронизировать время");
        }
    } else {
        ESP_LOGW(TAG, "SNTP: не дождались подключения к Wi-Fi");
    }
    vTaskDelete(NULL);
}

static void battery_task(void *arg)
{
    battery_info_t info;
    while (1) {
        if (battery_manager_get(&info) == ESP_OK) {
            bsp_display_lock(0);
            watch_ui_set_battery(info.level, info.charging || info.full);
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void wifi_icon_task(void *arg)
{
    while (1) {
        const app_config_t *cfg = config_get();
        bool connected = false;
        if (cfg->wifi_enabled) {
            connected = wifi_cfg_is_connected();
        }
        bsp_display_lock(0);
        watch_ui_set_wifi(connected);
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    config_load();
    const app_config_t *cfg = config_get();

    // Wi-Fi (только если включён)
    if (cfg->wifi_enabled) {
        ESP_LOGI(TAG, "Wi-Fi включён, запускаем WiFi Config");
        esp_bus_init();

        static wifi_network_t default_net = {0};
        strncpy(default_net.ssid, cfg->wifi_ssid, sizeof(default_net.ssid) - 1);
        strncpy(default_net.password, cfg->wifi_password, sizeof(default_net.password) - 1);
        default_net.priority = 10;

        wifi_cfg_config_t wifi_cfg_config = WIFI_CFG_DEFAULT_CONFIG();
        wifi_cfg_config.default_networks = (strlen(cfg->wifi_ssid) > 0) ? &default_net : NULL;
        wifi_cfg_config.default_network_count = (strlen(cfg->wifi_ssid) > 0) ? 1 : 0;
        wifi_cfg_config.stop_provisioning_on_connect = true;
        wifi_cfg_config.enable_ap = true;
        strncpy(wifi_cfg_config.default_ap.ssid, "ensmith-watch", sizeof(wifi_cfg_config.default_ap.ssid) - 1);
        wifi_cfg_config.default_ap.ssid[sizeof(wifi_cfg_config.default_ap.ssid) - 1] = '\0';

        ESP_LOGI(TAG, "Запуск Wi-Fi и точки доступа ensmith-watch...");
        // wifi_cfg_wait_connected(30000); // Убрано, чтобы не блокировать загрузку

        wifi_cfg_init(&wifi_cfg_config);
        web_api_init();
    } else {
        ESP_LOGI(TAG, "Wi-Fi выключен в настройках — пропускаем");
    }

    // Дисплей
    bsp_display_start();
    ESP_LOGI(TAG, "Display started");

    bsp_display_brightness_init();
    bsp_display_brightness_set(cfg->brightness);
    ESP_LOGI(TAG, "Яркость: %d%%", cfg->brightness);

    // RTC
    ret = rtc_manager_init();
    if (ret == ESP_OK) {
        rtc_time_t now;
        if (rtc_manager_get_time(&now) == ESP_OK) {
            ESP_LOGI(TAG, "RTC время: %04d-%02d-%02d %02d:%02d:%02d",
                     now.year, now.month, now.day, now.hour, now.minute, now.second);
        }
    }

    // Батарея
    ret = battery_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Battery manager init failed");
    }

    // IMU
    ret = power_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Power manager init failed");
    }

    // SNTP
    if (cfg->wifi_enabled) {
        xTaskCreate(sntp_task, "sntp", 8192, NULL, 4, NULL);
    }

    // UI
    bsp_display_lock(0);
    screen_manager_init();

    s_watch_screen = watch_ui_create();
    screen_manager_register(SCREEN_WATCH, s_watch_screen);

    menu_ui_create();
    screen_manager_register(SCREEN_MENU, menu_ui_get_screen());

    about_ui_create();
    screen_manager_register(SCREEN_ABOUT, about_ui_get_screen());

    calculator_ui_create();
    screen_manager_register(SCREEN_CALC, calculator_ui_get_screen());

    timer_ui_create();
    screen_manager_register(SCREEN_TIMER, timer_ui_get_screen());

    speedtest_ui_create();
    screen_manager_register(SCREEN_SPEEDTEST, speedtest_ui_get_screen());

    lv_obj_t *s_speed_screen = speedtest_ui_create_speed_screen();
    screen_manager_register(SCREEN_NET_SPEED, s_speed_screen);

    settings_ui_create();
    screen_manager_register(SCREEN_SETTINGS, settings_ui_get_screen());

    settings_wifi_info_create();
    screen_manager_register(SCREEN_WIFI_INFO, settings_wifi_info_get_screen());

    settings_key_create();
    screen_manager_register(SCREEN_KEY, settings_key_get_screen());

    settings_brightness_create();
    screen_manager_register(SCREEN_BRIGHTNESS, settings_brightness_get_screen());

    settings_timezone_create();
    screen_manager_register(SCREEN_TIMEZONE, settings_timezone_get_screen());

    settings_reset_create();
    screen_manager_register(SCREEN_RESET, settings_reset_get_screen());

        
    screen_manager_switch(SCREEN_WATCH);

    if (cfg->wifi_enabled) {
        watch_ui_set_wifi(wifi_cfg_is_connected());
    } else {
        watch_ui_set_wifi(false);
    }

    battery_info_t binfo;
    if (battery_manager_get(&binfo) == ESP_OK) {
        watch_ui_set_battery(binfo.level, binfo.charging || binfo.full);
    }

    bsp_display_unlock();

    xTaskCreate(watch_task, "watch", 4096, NULL, 3, NULL);
    xTaskCreate(battery_task, "battery", 4096, NULL, 2, NULL);
    xTaskCreate(wifi_icon_task, "wifi_icon", 4096, NULL, 2, NULL);
    power_manager_start();
    ESP_LOGI(TAG, "Готов к работе");
}
