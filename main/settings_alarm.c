#include "settings_alarm.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "rtc_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include <stdio.h>
#include <stdbool.h>

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_48);

static const char *TAG = "ALARM";

#define NVS_NS "watch_cfg"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_hour_label = NULL;
static lv_obj_t *s_min_label = NULL;
static lv_obj_t *s_enable_label = NULL;

static int  s_hour = 7;
static int  s_min = 0;
static bool s_enabled = false;

static void save_alarm(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_i32(h, "alarm_hour", s_hour);
    nvs_set_i32(h, "alarm_min",  s_min);
    nvs_set_u8 (h, "alarm_en",   s_enabled ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

static void load_alarm(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;

    int32_t v;
    if (nvs_get_i32(h, "alarm_hour", &v) == ESP_OK) s_hour = v;
    if (nvs_get_i32(h, "alarm_min",  &v) == ESP_OK) s_min = v;
    uint8_t en = 0;
    if (nvs_get_u8(h, "alarm_en", &en) == ESP_OK) s_enabled = (en != 0);
    nvs_close(h);
}

static void update_display(void)
{
    char buf[8];

    snprintf(buf, sizeof(buf), "%02d", s_hour);
    lv_label_set_text(s_hour_label, buf);

    snprintf(buf, sizeof(buf), "%02d", s_min);
    lv_label_set_text(s_min_label, buf);

    if (s_enabled) {
        lv_label_set_text(s_enable_label, "ВКЛ");
        lv_obj_set_style_text_color(s_enable_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(s_enable_label, "ВЫКЛ");
        lv_obj_set_style_text_color(s_enable_label, lv_color_hex(0xFF4444), 0);
    }
}

static void hour_up_cb(lv_event_t *e)
{
    s_hour = (s_hour + 1) % 24;
    update_display();
    save_alarm();
}
static void hour_down_cb(lv_event_t *e)
{
    s_hour = (s_hour + 23) % 24;
    update_display();
    save_alarm();
}

static void min_up_cb(lv_event_t *e)
{
    s_min = (s_min + 5) % 60;
    update_display();
    save_alarm();
}
static void min_down_cb(lv_event_t *e)
{
    s_min = (s_min + 55) % 60;
    update_display();
    save_alarm();
}

static void toggle_cb(lv_event_t *e)
{
    s_enabled = !s_enabled;
    update_display();
    save_alarm();
    ESP_LOGI(TAG, "Будильник: %s %02d:%02d",
             s_enabled ? "ВКЛ" : "ВЫКЛ", s_hour, s_min);
}

static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_MENU);
}

static void alarm_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void alarm_check_task(void *arg)
{
    int last_min = -1;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (!s_enabled) continue;

        rtc_time_t now;
        if (rtc_manager_get_time(&now) != ESP_OK) continue;

        const app_config_t *cfg = config_get();
        int local_hour = (now.hour + cfg->timezone_offset + 24) % 24;
        int local_min  = now.minute;

        if (local_min == last_min) continue;
        last_min = local_min;

        if (local_hour == s_hour && local_min == s_min) {
            ESP_LOGW(TAG, "БУДИЛЬНИК! %02d:%02d", s_hour, s_min);
            if (s_screen) {
                bsp_display_lock(0);
                lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x440000), 0);
                bsp_display_unlock();
                vTaskDelay(pdMS_TO_TICKS(3000));
                bsp_display_lock(0);
                lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
                bsp_display_unlock();
            }
        }
    }
}

lv_obj_t *settings_alarm_create(void)
{
    load_alarm();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, alarm_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Будильник");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    s_hour_label = lv_label_create(s_screen);
    lv_label_set_text(s_hour_label, "07");
    lv_obj_set_style_text_color(s_hour_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_hour_label, &font_ru_48, 0);
    lv_obj_align(s_hour_label, LV_ALIGN_TOP_LEFT, 60, 90);

    lv_obj_t *h_up = lv_button_create(s_screen);
    lv_obj_set_size(h_up, 50, 50);
    lv_obj_align(h_up, LV_ALIGN_TOP_LEFT, 70, 160);
    lv_obj_set_style_bg_color(h_up, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(h_up, hour_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *h_up_l = lv_label_create(h_up);
    lv_label_set_text(h_up_l, LV_SYMBOL_UP);
    lv_obj_center(h_up_l);

    lv_obj_t *h_dn = lv_button_create(s_screen);
    lv_obj_set_size(h_dn, 50, 50);
    lv_obj_align(h_dn, LV_ALIGN_TOP_LEFT, 70, 220);
    lv_obj_set_style_bg_color(h_dn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(h_dn, hour_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *h_dn_l = lv_label_create(h_dn);
    lv_label_set_text(h_dn_l, LV_SYMBOL_DOWN);
    lv_obj_center(h_dn_l);

    lv_obj_t *colon = lv_label_create(s_screen);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, lv_color_white(), 0);
    lv_obj_set_style_text_font(colon, &font_ru_48, 0);
    lv_obj_align(colon, LV_ALIGN_TOP_MID, 0, 90);

    s_min_label = lv_label_create(s_screen);
    lv_label_set_text(s_min_label, "00");
    lv_obj_set_style_text_color(s_min_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_min_label, &font_ru_48, 0);
    lv_obj_align(s_min_label, LV_ALIGN_TOP_RIGHT, -60, 90);

    lv_obj_t *m_up = lv_button_create(s_screen);
    lv_obj_set_size(m_up, 50, 50);
    lv_obj_align(m_up, LV_ALIGN_TOP_RIGHT, -70, 160);
    lv_obj_set_style_bg_color(m_up, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(m_up, min_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *m_up_l = lv_label_create(m_up);
    lv_label_set_text(m_up_l, LV_SYMBOL_UP);
    lv_obj_center(m_up_l);

    lv_obj_t *m_dn = lv_button_create(s_screen);
    lv_obj_set_size(m_dn, 50, 50);
    lv_obj_align(m_dn, LV_ALIGN_TOP_RIGHT, -70, 220);
    lv_obj_set_style_bg_color(m_dn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(m_dn, min_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *m_dn_l = lv_label_create(m_dn);
    lv_label_set_text(m_dn_l, LV_SYMBOL_DOWN);
    lv_obj_center(m_dn_l);

    s_enable_label = lv_label_create(s_screen);
    lv_label_set_text(s_enable_label, "ВЫКЛ");
    lv_obj_set_style_text_font(s_enable_label, &font_ru_24, 0);
    lv_obj_align(s_enable_label, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *toggle_btn = lv_button_create(s_screen);
    lv_obj_set_size(toggle_btn, 160, 50);
    lv_obj_align(toggle_btn, LV_ALIGN_BOTTOM_MID, -90, -30);
    lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(toggle_btn, toggle_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *toggle_l = lv_label_create(toggle_btn);
    lv_label_set_text(toggle_l, "ВКЛ / ВЫКЛ");
    lv_obj_set_style_text_font(toggle_l, &font_ru_14, 0);
    lv_obj_center(toggle_l);

    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 160, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 90, -30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_l = lv_label_create(back_btn);
    lv_label_set_text(back_l, "Назад");
    lv_obj_set_style_text_font(back_l, &font_ru_14, 0);
    lv_obj_center(back_l);

    xTaskCreate(alarm_check_task, "alarm_chk", 4096, NULL, 2, NULL);

    update_display();
    ESP_LOGI(TAG, "Экран будильника создан (%02d:%02d, %s)",
             s_hour, s_min, s_enabled ? "ВКЛ" : "ВЫКЛ");
    return s_screen;
}

lv_obj_t *settings_alarm_get_screen(void)
{
    return s_screen;
}
