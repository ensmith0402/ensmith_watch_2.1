#include "settings_reset.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "SETTINGS_RESET";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;

static void reset_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_SETTINGS);
    }
}

static void cancel_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_SETTINGS);
}

static void confirm_btn_cb(lv_event_t *e)
{
    ESP_LOGW(TAG, "Сброс настроек...");

    lv_label_set_text(s_status_label, "Сброс...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFAA00), 0);

    esp_err_t ret = config_reset();
    if (ret == ESP_OK) {
        lv_label_set_text(s_status_label, "Сброшено! Перезагрузка...");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
        ESP_LOGI(TAG, "NVS очищена, перезагрузка через 2 сек");

        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        lv_label_set_text(s_status_label, "Ошибка сброса");
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF4444), 0);
        ESP_LOGE(TAG, "config_reset failed: %s", esp_err_to_name(ret));
    }
}

lv_obj_t *settings_reset_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, reset_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Сброс");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *warn_label = lv_label_create(s_screen);
    lv_label_set_text(warn_label,
        "Будут удалены:\n"
        "• Wi-Fi (SSID и пароль)\n"
        "• API-ключ DeepSeek\n"
        "• Яркость\n"
        "• Часовой пояс\n\n"
        "Время и дата сохранятся.");
    lv_obj_set_style_text_color(warn_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(warn_label, &font_ru_14, 0);
    lv_obj_set_style_text_align(warn_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(warn_label, 360);
    lv_obj_align(warn_label, LV_ALIGN_TOP_LEFT, 25, 70);

    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_14, 0);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -100);

    lv_obj_t *cancel_btn = lv_button_create(s_screen);
    lv_obj_set_size(cancel_btn, 160, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_MID, -90, -30);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(cancel_btn, cancel_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Отмена");
    lv_obj_set_style_text_font(cancel_label, &font_ru_14, 0);
    lv_obj_clear_flag(cancel_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(cancel_label);

    lv_obj_t *confirm_btn = lv_button_create(s_screen);
    lv_obj_set_size(confirm_btn, 160, 50);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_MID, 90, -30);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0xCC0000), 0);
    lv_obj_add_event_cb(confirm_btn, confirm_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, "Сбросить");
    lv_obj_set_style_text_font(confirm_label, &font_ru_14, 0);
    lv_obj_clear_flag(confirm_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(confirm_label);

    ESP_LOGI(TAG, "Экран сброса создан");
    return s_screen;
}

lv_obj_t *settings_reset_get_screen(void)
{
    return s_screen;
}
