#include "about_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "battery_manager.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "ABOUT_UI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_info_label = NULL;

static void about_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_MENU);
}

lv_obj_t *about_ui_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, about_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "О часах");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    s_info_label = lv_label_create(s_screen);
    lv_label_set_text(s_info_label, "Загрузка...");
    lv_obj_set_style_text_color(s_info_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_info_label, &font_ru_14, 0);
    lv_obj_set_width(s_info_label, 380);
    lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_info_label, LV_ALIGN_TOP_LEFT, 15, 60);

    lv_obj_t *btn = lv_button_create(s_screen);
    lv_obj_set_size(btn, 120, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Назад");
    lv_obj_set_style_text_font(btn_label, &font_ru_14, 0);
    lv_obj_center(btn_label);

    ESP_LOGI(TAG, "Экран 'О часах' создан");
    return s_screen;
}

void about_ui_update(void)
{
    if (!s_info_label) return;

    char buf[512];
    int pos = 0;

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "Чип: ESP32-S3, %d ядер, ревизия %d\n",
                    chip.cores, chip.revision);

    pos += snprintf(buf + pos, sizeof(buf) - pos, "Flash: 16 MB\n");

    size_t psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "PSRAM: %d KB\n", (int)(psram / 1024));

    size_t free_heap = esp_get_free_heap_size();
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "Свободно RAM: %d KB\n", (int)(free_heap / 1024));

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            "IP: " IPSTR "\n", IP2STR(&ip.ip));
        } else {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "IP: не подключён\n");
        }
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    battery_info_t binfo;
    if (battery_manager_get(&binfo) == ESP_OK) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "Батарея: %d%%, %d mV\n",
                        binfo.level, binfo.voltage_mv);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "\nПрошивка: v1.0\nensmith watch\n");

    lv_label_set_text(s_info_label, buf);
}

lv_obj_t *about_ui_get_screen(void)
{
    return s_screen;
}
