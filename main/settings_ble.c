#include "fonts.h"
#include "settings_ble.h"
#include "screen_manager.h"
#include "ble_manager.h"
#include "esp_log.h"
LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_20);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "SETTINGS_BLE";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_info_label = NULL;

static void ble_gesture_cb(lv_event_t *e)
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

void settings_ble_update_status(void)
{
    if (!s_status_label) return;

    if (ble_manager_is_connected()) {
        lv_label_set_text(s_status_label, "Подключено");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(s_status_label, "Ожидание подключения...");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFAA00), 0);
    }
}

lv_obj_t *settings_ble_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, ble_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Информация
    s_info_label = lv_label_create(s_screen);
    lv_label_set_text(s_info_label,
        "Имя: ensmith watch\n\n"
        "Подключитесь через BLE-\n"
        "приложение (nRF Connect,\n"
        "LightBlue).");
    lv_obj_set_style_text_color(s_info_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_info_label, &font_ru_18, 0);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_info_label, LV_ALIGN_CENTER, 0, -50);

    // Статус
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Ожидание подключения...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_20, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 70);

    // Кнопка "Назад"
    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 160, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Экран BLE создан");
    return s_screen;
}

lv_obj_t *settings_ble_get_screen(void)
{
    return s_screen;
}
