#include "settings_ui.h"
#include "screen_manager.h"
#include "settings_wifi_info.h"
#include "esp_log.h"
#include <string.h>

LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "SETTINGS_UI";

static lv_obj_t *s_screen = NULL;

static void settings_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → часы");
        screen_manager_switch(SCREEN_WATCH);
    }
}

static void settings_item_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Настройки → %s", name);

    if (strcmp(name, "Яркость") == 0) {
        screen_manager_switch(SCREEN_BRIGHTNESS);
    } else if (strcmp(name, "Wi-Fi") == 0) {
        settings_wifi_info_update();
        screen_manager_switch(SCREEN_WIFI_INFO);
    } else if (strcmp(name, "API-ключ") == 0) {
        screen_manager_switch(SCREEN_KEY);
    } else if (strcmp(name, "Часовой пояс") == 0) {
        screen_manager_switch(SCREEN_TIMEZONE);
    } else if (strcmp(name, "Сброс") == 0) {
        screen_manager_switch(SCREEN_RESET);
    } else if (strcmp(name, "Назад") == 0) {
        screen_manager_switch(SCREEN_WATCH); // Возврат на циферблат
    }
}

static void create_list_item(lv_obj_t *parent, const char *symbol,
                              const char *label, lv_coord_t y)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 380, 60);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 12, 0);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *text = lv_label_create(btn);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_color(text, lv_color_white(), 0);
    lv_obj_set_style_text_font(text, &font_ru_18, 0);
    lv_obj_align(text, LV_ALIGN_LEFT_MID, 60, 0);

    lv_obj_t *arrow = lv_label_create(btn);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x888888), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_add_event_cb(btn, settings_item_cb, LV_EVENT_CLICKED, (void *)label);
}

lv_obj_t *settings_ui_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, settings_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Настройки");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    create_list_item(s_screen, LV_SYMBOL_IMAGE,    "Яркость",       70);
    create_list_item(s_screen, LV_SYMBOL_WIFI,     "Wi-Fi",         140);
    create_list_item(s_screen, LV_SYMBOL_KEYBOARD, "API-ключ",      210);
    create_list_item(s_screen, LV_SYMBOL_GPS,      "Часовой пояс",  280);
    create_list_item(s_screen, LV_SYMBOL_TRASH,    "Сброс",         350);
    create_list_item(s_screen, LV_SYMBOL_LEFT,     "Назад",         420);

    ESP_LOGI(TAG, "Экран настроек создан");
    return s_screen;
}

lv_obj_t *settings_ui_get_screen(void)
{
    return s_screen;
}
