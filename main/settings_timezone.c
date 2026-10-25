#include "settings_timezone.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_32);

static const char *TAG = "SETTINGS_TZ";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_value_label = NULL;
static int8_t s_offset = 3;

static const char *tz_name(int8_t offset)
{
    switch (offset) {
        case 2:  return "Калининград";
        case 3:  return "Москва";
        case 4:  return "Самара";
        case 5:  return "Екатеринбург";
        case 6:  return "Омск";
        case 7:  return "Новосибирск";
        case 8:  return "Иркутск";
        case 9:  return "Якутск";
        case 10: return "Владивосток";
        case 11: return "Магадан";
        case 12: return "Камчатка";
        default: return "";
    }
}

static void tz_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_SETTINGS);
    }
}

static void update_value_label(void)
{
    char buf[64];
    const char *name = tz_name(s_offset);
    if (name[0] != '\0') {
        snprintf(buf, sizeof(buf), "UTC%+d\n%s", s_offset, name);
    } else {
        snprintf(buf, sizeof(buf), "UTC%+d", s_offset);
    }
    lv_label_set_text(s_value_label, buf);
}

static void up_btn_cb(lv_event_t *e)
{
    if (s_offset < 14) {
        s_offset++;
        update_value_label();
    }
}

static void down_btn_cb(lv_event_t *e)
{
    if (s_offset > -12) {
        s_offset--;
        update_value_label();
    }
}

static void save_btn_cb(lv_event_t *e)
{
    config_set_timezone(s_offset);
    ESP_LOGI(TAG, "Часовой пояс сохранён: UTC%+d", s_offset);
    screen_manager_switch(SCREEN_SETTINGS);
}

static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_SETTINGS);
}

lv_obj_t *settings_timezone_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, tz_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Часовой пояс");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    s_value_label = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_value_label, &font_ru_32, 0);
    lv_obj_set_style_text_align(s_value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_value_label, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *up_btn = lv_button_create(s_screen);
    lv_obj_set_size(up_btn, 80, 80);
    lv_obj_align(up_btn, LV_ALIGN_CENTER, -120, -30);
    lv_obj_set_style_bg_color(up_btn, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(up_btn, 40, 0);
    lv_obj_add_event_cb(up_btn, up_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *up_label = lv_label_create(up_btn);
    lv_label_set_text(up_label, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(up_label, &lv_font_montserrat_24, 0);
    lv_obj_center(up_label);

    lv_obj_t *down_btn = lv_button_create(s_screen);
    lv_obj_set_size(down_btn, 80, 80);
    lv_obj_align(down_btn, LV_ALIGN_CENTER, 120, -30);
    lv_obj_set_style_bg_color(down_btn, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(down_btn, 40, 0);
    lv_obj_add_event_cb(down_btn, down_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *down_label = lv_label_create(down_btn);
    lv_label_set_text(down_label, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(down_label, &lv_font_montserrat_24, 0);
    lv_obj_center(down_label);

    lv_obj_t *save_btn = lv_button_create(s_screen);
    lv_obj_set_size(save_btn, 160, 50);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, -90, -30);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Сохранить");
    lv_obj_set_style_text_font(save_label, &font_ru_14, 0);
    lv_obj_center(save_label);

    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 160, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 90, -30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_set_style_text_font(back_label, &font_ru_14, 0);
    lv_obj_center(back_label);

    const app_config_t *cfg = config_get();
    s_offset = cfg->timezone_offset;
    update_value_label();

    ESP_LOGI(TAG, "Экран часового пояса создан (UTC%+d)", s_offset);
    return s_screen;
}

lv_obj_t *settings_timezone_get_screen(void)
{
    return s_screen;
}
