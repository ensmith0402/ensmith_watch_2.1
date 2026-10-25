#include "settings_wifi_info.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "watch_ui.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "WIFI_INFO";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_info_label = NULL;
static lv_obj_t *s_toggle_btn_label = NULL;

/* ---------- Свайп вправо → настройки ---------- */
static void wifi_info_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → настройки");
        screen_manager_switch(SCREEN_SETTINGS);
    }
}

/* ---------- Обновление информации о Wi-Fi ---------- */
void settings_wifi_info_update(void)
{
    if (!s_status_label || !s_info_label) return;

    const app_config_t *cfg = config_get();

    if (s_toggle_btn_label) {
        lv_label_set_text(s_toggle_btn_label, cfg->wifi_enabled ? "Выключить Wi-Fi" : "Включить Wi-Fi");
    }

    if (!cfg->wifi_enabled) {
        lv_label_set_text(s_status_label, "Wi-Fi: выключен");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF4444), 0);
        lv_label_set_text(s_info_label,
            "Wi-Fi отключён для\n"
            "экономии батареи.\n\n"
            "Нажмите кнопку, чтобы\n"
            "включить Wi-Fi.");
        return;
    }

    wifi_ap_record_t ap_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret != ESP_OK) {
        lv_label_set_text(s_status_label, "Wi-Fi: не подключён");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF4444), 0);
        lv_label_set_text(s_info_label,
            "Wi-Fi включён, но нет\n"
            "подключения.\n\n"
            "Подключитесь к точке\n"
            "доступа ensmith-watch\n"
            "с телефона.");
        return;
    }

    char ip_str[16] = "---";
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip.ip));
        }
    }

    lv_label_set_text(s_status_label, "Wi-Fi: подключён");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "SSID: %s\n"
        "IP:   %s\n"
        "RSSI: %d dBm\n"
        "Канал: %d",
        (char *)ap_info.ssid,
        ip_str,
        ap_info.rssi,
        ap_info.primary);

    lv_label_set_text(s_info_label, buf);

    ESP_LOGI(TAG, "Wi-Fi info updated: SSID=%s, IP=%s, RSSI=%d",
             (char *)ap_info.ssid, ip_str, ap_info.rssi);
}

/* ---------- Кнопка ВКЛ/ВЫКЛ Wi-Fi ---------- */
static void toggle_wifi_cb(lv_event_t *e)
{
    const app_config_t *cfg = config_get();
    bool new_state = !cfg->wifi_enabled;

    config_set_wifi_enabled(new_state);
    ESP_LOGI(TAG, "Wi-Fi: %s", new_state ? "включён" : "выключен");

    if (new_state) {
        esp_wifi_start();
    } else {
        esp_wifi_stop();
        // Обновляем иконку на главном экране — Wi-Fi выключен
        bsp_display_lock(0);
        watch_ui_set_wifi(false);
        bsp_display_unlock();
    }

    settings_wifi_info_update();
}

/* ---------- Кнопка "Назад" ---------- */
static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_SETTINGS);
}

lv_obj_t *settings_wifi_info_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, wifi_info_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Wi-Fi");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Wi-Fi: загрузка...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_24, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 60);

    s_info_label = lv_label_create(s_screen);
    lv_label_set_text(s_info_label, "");
    lv_obj_set_style_text_color(s_info_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_info_label, &font_ru_24, 0);
    lv_obj_set_width(s_info_label, 380);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_info_label, LV_ALIGN_TOP_LEFT, 20, 120);

    lv_obj_t *toggle_btn = lv_button_create(s_screen);
    lv_obj_set_size(toggle_btn, 300, 60);
    lv_obj_align(toggle_btn, LV_ALIGN_BOTTOM_MID, 0, -90);
    lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(toggle_btn, toggle_wifi_cb, LV_EVENT_CLICKED, NULL);
    s_toggle_btn_label = lv_label_create(toggle_btn);
    lv_label_set_text(s_toggle_btn_label, "Включить Wi-Fi");
    lv_obj_set_style_text_font(s_toggle_btn_label, &font_ru_24, 0);
    lv_obj_center(s_toggle_btn_label);

    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 200, 55);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_set_style_text_font(back_label, &font_ru_24, 0);
    lv_obj_center(back_label);

    settings_wifi_info_update();

    ESP_LOGI(TAG, "Экран Wi-Fi создан");
    return s_screen;
}

lv_obj_t *settings_wifi_info_get_screen(void)
{
    return s_screen;
}
