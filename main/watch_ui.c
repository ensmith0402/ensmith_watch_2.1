#include "watch_ui.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_32);
LV_FONT_DECLARE(font_ru_48);
LV_FONT_DECLARE(font_ru_96);
LV_FONT_DECLARE(font_seg_115);
LV_FONT_DECLARE(font_seg_40);

static const char *TAG = "WATCH_UI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_weekday_label = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static lv_obj_t *s_battery_icon = NULL;
static lv_obj_t *s_battery_label = NULL;

static const char *WEEKDAYS_RU[] = {
    "Воскресенье", "Понедельник", "Вторник", "Среда",
    "Четверг", "Пятница", "Суббота"
};

/* ---------- Обработчик свайпов ---------- */
static void watch_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → меню");
        screen_manager_switch(SCREEN_MENU);
    } else if (dir == LV_DIR_LEFT) {
        ESP_LOGI(TAG, "Свайп влево → спидометр");
        screen_manager_switch(SCREEN_SPEEDTEST);
    }
}

/* ---------- Status bar (иконки по центру) ---------- */
static void create_status_bar(lv_obj_t *parent)
{
    s_wifi_icon = lv_label_create(parent);
    lv_label_set_text(s_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_wifi_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(s_wifi_icon, LV_ALIGN_TOP_MID, -40, 10);

    s_battery_icon = lv_label_create(parent);
    lv_label_set_text(s_battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(s_battery_icon, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_battery_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(s_battery_icon, LV_ALIGN_TOP_MID, 10, 10);

    s_battery_label = lv_label_create(parent);
    lv_label_set_text(s_battery_label, "--%");
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_MID, 55, 10);
}

lv_obj_t *watch_ui_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, watch_gesture_cb, LV_EVENT_GESTURE, NULL);

    create_status_bar(s_screen);

    // Время (огромный шрифт, только ЧЧ:ММ)
    s_time_label = lv_label_create(s_screen);
    lv_label_set_text(s_time_label, "00:00");
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_label, &font_seg_115, 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -70);

    // Дата (шрифт 48)
    s_date_label = lv_label_create(s_screen);
    lv_label_set_text(s_date_label, "01.01.2026");
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(s_date_label, &font_seg_40, 0);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 60);

    // День недели (шрифт 32)
    s_weekday_label = lv_label_create(s_screen);
    lv_label_set_text(s_weekday_label, "Понедельник");
    lv_obj_set_style_text_color(s_weekday_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_weekday_label, &font_ru_32, 0);
    lv_obj_align(s_weekday_label, LV_ALIGN_CENTER, 0, 130);

    ESP_LOGI(TAG, "Экран часов создан");
    return s_screen;
}

void watch_ui_update(const rtc_time_t *time_utc)
{
    if (!s_time_label || !time_utc) return;

    const app_config_t *cfg = config_get();
    int8_t tz = cfg->timezone_offset;

    int hour = time_utc->hour + tz;
    int day = time_utc->day;
    int month = time_utc->month;
    int year = time_utc->year;
    int weekday = time_utc->weekday;

    if (hour >= 24) {
        hour -= 24;
        day += 1;
        weekday = (weekday + 1) % 7;
    } else if (hour < 0) {
        hour += 24;
        day -= 1;
        weekday = (weekday + 6) % 7;
    }

    char buf[32];

    snprintf(buf, sizeof(buf), "%02d:%02d", hour, time_utc->minute);
    lv_label_set_text(s_time_label, buf);

    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", day, month, year);
    lv_label_set_text(s_date_label, buf);

    if (weekday >= 0 && weekday <= 6) {
        lv_label_set_text(s_weekday_label, WEEKDAYS_RU[weekday]);
    }
}

void watch_ui_set_battery(int level, bool charging)
{
    if (!s_battery_icon || !s_battery_label) return;

    const char *symbol;
    if (charging) {
        symbol = LV_SYMBOL_CHARGE;
    } else if (level < 0) {
        symbol = LV_SYMBOL_BATTERY_EMPTY;
    } else if (level >= 90) {
        symbol = LV_SYMBOL_BATTERY_FULL;
    } else if (level >= 60) {
        symbol = LV_SYMBOL_BATTERY_3;
    } else if (level >= 35) {
        symbol = LV_SYMBOL_BATTERY_2;
    } else if (level >= 15) {
        symbol = LV_SYMBOL_BATTERY_1;
    } else {
        symbol = LV_SYMBOL_BATTERY_EMPTY;
    }

    lv_label_set_text(s_battery_icon, symbol);

    lv_color_t color;
    if (charging) {
        color = lv_color_hex(0x00FF00);
    } else if (level >= 0 && level < 15) {
        color = lv_color_hex(0xFF4444);
    } else {
        color = lv_color_hex(0x888888);
    }
    lv_obj_set_style_text_color(s_battery_icon, color, 0);

    char buf[8];
    if (level >= 0) {
        snprintf(buf, sizeof(buf), "%d%%", level);
    } else {
        snprintf(buf, sizeof(buf), "--%%");
    }
    lv_label_set_text(s_battery_label, buf);
    lv_obj_set_style_text_color(s_battery_label, color, 0);
}

void watch_ui_set_wifi(bool connected)
{
    if (!s_wifi_icon) return;

    lv_label_set_text(s_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_icon,
        connected ? lv_color_hex(0x00FF00) : lv_color_hex(0x444444), 0);
}

lv_obj_t *watch_ui_get_screen(void)
{
    return s_screen;
}
