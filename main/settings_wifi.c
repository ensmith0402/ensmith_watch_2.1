#include "fonts.h"
#include "settings_wifi.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include <string.h>
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "SETTINGS_WIFI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_ssid_ta = NULL;
static lv_obj_t *s_pass_ta = NULL;
static lv_obj_t *s_keyboard = NULL;
static lv_obj_t *s_status_label = NULL;

/* ---------- Свайп вправо → настройки ---------- */
static void wifi_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → настройки");
        screen_manager_switch(SCREEN_SETTINGS);
    }
}

/* ---------- Фокус на поле ввода ---------- */
static void ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        if (s_keyboard) {
            lv_keyboard_set_textarea(s_keyboard, ta);
            lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY) {
        if (s_keyboard) {
            lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ---------- Кнопка "Сохранить" ---------- */
static void save_btn_cb(lv_event_t *e)
{
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);

    if (!ssid || strlen(ssid) == 0) {
        lv_label_set_text(s_status_label, "SSID не может быть пустым");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF4444), 0);
        return;
    }

    esp_err_t ret = config_set_wifi(ssid, pass);
    if (ret == ESP_OK) {
        lv_label_set_text(s_status_label, "Сохранено! Перезагрузите часы.");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
        ESP_LOGI(TAG, "Wi-Fi сохранён: SSID='%s'", ssid);
    } else {
        lv_label_set_text(s_status_label, "Ошибка сохранения");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF4444), 0);
    }
}

/* ---------- Кнопка "Назад" ---------- */
static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_SETTINGS);
}

lv_obj_t *settings_wifi_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, wifi_gesture_cb, LV_EVENT_GESTURE, NULL);

    // Заголовок
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Wi-Fi");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Поле SSID
    lv_obj_t *ssid_label = lv_label_create(s_screen);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 65);

    s_ssid_ta = lv_textarea_create(s_screen);
    lv_textarea_set_one_line(s_ssid_ta, true);
    lv_textarea_set_placeholder_text(s_ssid_ta, "Имя сети");
    lv_obj_set_width(s_ssid_ta, 370);
    lv_obj_align(s_ssid_ta, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_event_cb(s_ssid_ta, ta_event_cb, LV_EVENT_ALL, NULL);

    // Поле пароля
    lv_obj_t *pass_label = lv_label_create(s_screen);
    lv_label_set_text(pass_label, "Пароль:");
    lv_obj_set_style_text_color(pass_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(pass_label, LV_ALIGN_TOP_LEFT, 20, 145);

    s_pass_ta = lv_textarea_create(s_screen);
    lv_textarea_set_one_line(s_pass_ta, true);
    lv_textarea_set_password_mode(s_pass_ta, true);
    lv_textarea_set_placeholder_text(s_pass_ta, "Пароль");
    lv_obj_set_width(s_pass_ta, 370);
    lv_obj_align(s_pass_ta, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_add_event_cb(s_pass_ta, ta_event_cb, LV_EVENT_ALL, NULL);

    // Статус
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 230);

    // Кнопка "Сохранить"
    lv_obj_t *save_btn = lv_button_create(s_screen);
    lv_obj_set_size(save_btn, 160, 50);
    lv_obj_align(save_btn, LV_ALIGN_TOP_MID, -90, 270);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Сохранить");
    lv_obj_center(save_label);

    // Кнопка "Назад"
    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 160, 50);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 90, 270);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_center(back_label);

    // Клавиатура
    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s_keyboard, LV_PCT(100), LV_PCT(45));
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_keyboard, s_ssid_ta);

    ESP_LOGI(TAG, "Экран Wi-Fi создан");
    return s_screen;
}

lv_obj_t *settings_wifi_get_screen(void)
{
    return s_screen;
}
