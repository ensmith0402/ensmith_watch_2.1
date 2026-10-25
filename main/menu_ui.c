#include "menu_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include <string.h>

LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "MENU_UI";

static lv_obj_t *s_menu_screen = NULL;

/* ---------- Обработчик свайпов ---------- */
static void menu_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → часы");
        screen_manager_switch(SCREEN_WATCH);
    }
}

/* ---------- Callback кнопок меню ---------- */
static void menu_btn_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Нажата кнопка: %s", name);

    if (strcmp(name, "Назад") == 0) {
        screen_manager_switch(SCREEN_WATCH);
    } else if (strcmp(name, "О часах") == 0) {
        screen_manager_switch(SCREEN_ABOUT);
    } else if (strcmp(name, "Настройки") == 0) {
        screen_manager_switch(SCREEN_SETTINGS);
    } else if (strcmp(name, "Таймер") == 0) {
        // TODO: экран таймера
    } else if (strcmp(name, "Будильник") == 0) {
        screen_manager_switch(SCREEN_ALARM);
    } else if (strcmp(name, "BLE") == 0) {
        // TODO: экран BLE
    }
}

/* ---------- Создание кнопки меню ---------- */
static void create_menu_button(lv_obj_t *parent, const char *symbol,
                                const char *label, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 110, 110);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 16, 0);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t *text = lv_label_create(btn);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_color(text, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(text, &font_ru_18, 0);
    lv_obj_align(text, LV_ALIGN_CENTER, 0, 25);

    lv_obj_add_event_cb(btn, menu_btn_cb, LV_EVENT_CLICKED, (void *)label);
}

lv_obj_t *menu_ui_create(void)
{
    s_menu_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_menu_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_menu_screen, 0, 0);
    lv_obj_set_style_pad_all(s_menu_screen, 0, 0);

    lv_obj_add_event_cb(s_menu_screen, menu_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_menu_screen);
    lv_label_set_text(title, "Меню");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    create_menu_button(s_menu_screen, LV_SYMBOL_BELL,     "Будильник", 25,  70);
    create_menu_button(s_menu_screen, LV_SYMBOL_PLAY,     "Таймер",    145, 70);
    create_menu_button(s_menu_screen, LV_SYMBOL_SETTINGS, "Настройки", 265, 70);

    create_menu_button(s_menu_screen, LV_SYMBOL_LIST,     "О часах",   25,  200);
    create_menu_button(s_menu_screen, LV_SYMBOL_LEFT,     "Назад",     145, 200);

    ESP_LOGI(TAG, "Экран меню создан");
    return s_menu_screen;
}

lv_obj_t *menu_ui_get_screen(void)
{
    return s_menu_screen;
}
