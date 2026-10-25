#include "chat_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include <string.h>

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "CHAT_UI";

static lv_obj_t *s_chat_screen = NULL;
static lv_obj_t *s_input_ta = NULL;
static lv_obj_t *s_response_label = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_keyboard = NULL;
static chat_send_cb_t s_send_cb = NULL;

static void chat_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_WATCH);
    }
}

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

static void send_btn_cb(lv_event_t *e)
{
    if (s_send_cb) {
        s_send_cb(chat_ui_get_input());
    }
}

void chat_ui_set_send_callback(chat_send_cb_t cb)
{
    s_send_cb = cb;
}

void chat_ui_create(void)
{
    s_chat_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_chat_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_chat_screen, 0, 0);
    lv_obj_set_style_pad_all(s_chat_screen, 0, 0);

    lv_obj_t *scr = s_chat_screen;

    lv_obj_add_event_cb(scr, chat_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "DeepSeek Chat");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Готов");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_14, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 40);

    s_response_label = lv_label_create(scr);
    lv_label_set_text(s_response_label, "Введите запрос...");
    lv_obj_set_style_text_color(s_response_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_response_label, &font_ru_14, 0);
    lv_obj_set_width(s_response_label, 380);
    lv_label_set_long_mode(s_response_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_response_label, LV_ALIGN_TOP_LEFT, 15, 80);

    s_input_ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(s_input_ta, true);
    lv_textarea_set_placeholder_text(s_input_ta, "Ваш вопрос...");
    lv_obj_set_width(s_input_ta, 260);
    lv_obj_align(s_input_ta, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(s_input_ta, ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 110, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn, send_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Отправить");
    lv_obj_set_style_text_font(btn_label, &font_ru_14, 0);
    lv_obj_center(btn_label);

    s_keyboard = lv_keyboard_create(scr);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s_keyboard, LV_PCT(100), LV_PCT(50));
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_keyboard, s_input_ta);

    ESP_LOGI(TAG, "UI чата создан");
}

void chat_ui_set_response(const char *text)
{
    if (!s_response_label || !text) return;
    lv_label_set_text(s_response_label, text);
}

void chat_ui_set_status(const char *text)
{
    if (!s_status_label || !text) return;
    lv_label_set_text(s_status_label, text);
}

const char *chat_ui_get_input(void)
{
    if (!s_input_ta) return "";
    return lv_textarea_get_text(s_input_ta);
}

void chat_ui_clear_input(void)
{
    if (s_input_ta) {
        lv_textarea_set_text(s_input_ta, "");
    }
}

lv_obj_t *chat_ui_get_screen(void)
{
    return s_chat_screen;
}
