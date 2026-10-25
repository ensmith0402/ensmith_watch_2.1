#include "settings_brightness.h"
#include "screen_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

LV_FONT_DECLARE(font_ru_14);
LV_FONT_DECLARE(font_ru_18);
LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_32);

static const char *TAG = "SETTINGS_BRIGHT";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_slider = NULL;
static lv_obj_t *s_value_label = NULL;

/* ---------- Свайп вправо → настройки ---------- */
static void bright_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → настройки");
        screen_manager_switch(SCREEN_SETTINGS);
    }
}

/* ---------- Слайдер: изменение ---------- */
static void slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        int val = lv_slider_get_value(s_slider);
        bsp_display_brightness_set(val);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(s_value_label, buf);
    } else if (code == LV_EVENT_RELEASED) {
        int val = lv_slider_get_value(s_slider);
        config_set_brightness((uint8_t)val);
        ESP_LOGI(TAG, "Яркость сохранена: %d%%", val);
    }
}

/* ---------- Кнопка "Назад" ---------- */
static void back_btn_cb(lv_event_t *e)
{
    screen_manager_switch(SCREEN_SETTINGS);
}

lv_obj_t *settings_brightness_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, bright_gesture_cb, LV_EVENT_GESTURE, NULL);

    // Заголовок
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Яркость");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Текущее значение
    s_value_label = lv_label_create(s_screen);
    lv_label_set_text(s_value_label, "80%");
    lv_obj_set_style_text_color(s_value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_value_label, &font_ru_32, 0);
    lv_obj_align(s_value_label, LV_ALIGN_CENTER, 0, -80);

    // Слайдер
    s_slider = lv_slider_create(s_screen);
    lv_obj_set_width(s_slider, 320);
    lv_obj_align(s_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(s_slider, 5, 100);
    lv_slider_set_value(s_slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider, slider_event_cb, LV_EVENT_ALL, NULL);

    // Подписи min/max
    lv_obj_t *min_label = lv_label_create(s_screen);
    lv_label_set_text(min_label, "5%");
    lv_obj_set_style_text_color(min_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(min_label, &font_ru_14, 0);
    lv_obj_align(min_label, LV_ALIGN_CENTER, -170, 30);

    lv_obj_t *max_label = lv_label_create(s_screen);
    lv_label_set_text(max_label, "100%");
    lv_obj_set_style_text_color(max_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(max_label, &font_ru_14, 0);
    lv_obj_align(max_label, LV_ALIGN_CENTER, 170, 30);

    // Кнопка "Назад"
    lv_obj_t *back_btn = lv_button_create(s_screen);
    lv_obj_set_size(back_btn, 160, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_set_style_text_font(back_label, &font_ru_14, 0);
    lv_obj_center(back_label);

    // Загружаем текущую яркость
    const app_config_t *cfg = config_get();
    lv_slider_set_value(s_slider, cfg->brightness, LV_ANIM_OFF);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", cfg->brightness);
    lv_label_set_text(s_value_label, buf);

    ESP_LOGI(TAG, "Экран яркости создан");
    return s_screen;
}

lv_obj_t *settings_brightness_get_screen(void)
{
    return s_screen;
}
