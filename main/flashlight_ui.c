#include "flashlight_ui.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "FLASHLIGHT_UI";
static lv_obj_t *s_screen = NULL;

static void exit_btn_cb(lv_event_t *e) {
    // Возвращаем яркость из настроек
    const app_config_t *cfg = config_get();
    bsp_display_brightness_set(cfg->brightness);
    
    // Возвращаемся на часы
    screen_manager_switch(SCREEN_WATCH);
}

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        exit_btn_cb(e);
    }
}

lv_obj_t *flashlight_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_white(), 0); // Белый фон
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    // Кнопка выхода (чтобы было понятно, как выключить)
    lv_obj_t *btn = lv_button_create(s_screen);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0); // Черная кнопка на белом фоне
    lv_obj_add_event_cb(btn, exit_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Выход");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &font_ru_24, 0);
    lv_obj_center(lbl);

    // Устанавливаем максимальную яркость
    bsp_display_brightness_set(100);
    ESP_LOGI(TAG, "Фонарик включен (яркость 100%)");

    return s_screen;
}

lv_obj_t *flashlight_ui_get_screen(void) {
    return s_screen;
}
