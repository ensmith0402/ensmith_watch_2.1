#include "flashlight_ui.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "FLASHLIGHT_UI";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_slider = NULL;
static lv_obj_t *s_val_label = NULL;

// Обработчик движения ползунка
static void slider_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        int val = lv_slider_get_value(s_slider);
        
        // Меняем яркость экрана
        bsp_display_brightness_set(val);
        
        // Сохраняем в конфиг, чтобы система знала новую яркость
        config_set_brightness((uint8_t)val);
        
        // Обновляем текст
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(s_val_label, buf);
    }
}

static void exit_flashlight(void) {
    screen_manager_switch(SCREEN_WATCH);
}

static void exit_btn_cb(lv_event_t *e) {
    exit_flashlight();
}

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        exit_flashlight();
    }
}

lv_obj_t *flashlight_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_white(), 0); // Белый фон
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    // Получаем текущую яркость из системы
    const app_config_t *cfg = config_get();
    int current_brightness = cfg->brightness;

    // Слайдер управления яркостью
    s_slider = lv_slider_create(s_screen);
    lv_obj_set_width(s_slider, 300);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_slider_set_range(s_slider, 1, 100);
    lv_slider_set_value(s_slider, current_brightness, LV_ANIM_OFF); // Ставим текущую яркость
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x444444), 0); // Темный фон слайдера
    lv_obj_add_event_cb(s_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Текст значения
    s_val_label = lv_label_create(s_screen);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", current_brightness);
    lv_label_set_text(s_val_label, buf);
    lv_obj_set_style_text_color(s_val_label, lv_color_black(), 0); // Черный текст на белом фоне
    lv_obj_set_style_text_font(s_val_label, &font_ru_24, 0);
    lv_obj_align_to(s_val_label, s_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Кнопка выхода
    lv_obj_t *btn = lv_button_create(s_screen);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_add_event_cb(btn, exit_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Выход");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &font_ru_24, 0);
    lv_obj_center(lbl);

    ESP_LOGI(TAG, "Фонарик создан. Текущая яркость: %d%%", current_brightness);
    return s_screen;
}

lv_obj_t *flashlight_ui_get_screen(void) {
    return s_screen;
}
