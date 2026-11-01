#include "timer_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_seg_115);

static const char *TAG = "TIMER_UI";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_time_label = NULL;
static int s_remaining_sec = 0;
static bool s_timer_running = false;
static TaskHandle_t s_timer_task_handle = NULL;

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void update_display(void) {
    char buf[16];
    int h = s_remaining_sec / 3600;
    int m = (s_remaining_sec % 3600) / 60;
    int s = s_remaining_sec % 60;
    
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    }
    
    bsp_display_lock(0);
    lv_label_set_text(s_time_label, buf);
    bsp_display_unlock();
}

static void timer_task(void *pvParameters) {
    while (s_timer_running && s_remaining_sec > 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!s_timer_running) break;
        s_remaining_sec--;
        update_display();
    }
    
    if (s_remaining_sec <= 0 && s_timer_running) {
        s_timer_running = false;
        bsp_display_lock(0);
        lv_label_set_text(s_time_label, "Время!");
        bsp_display_unlock();
        // Тут можно добавить звуковой сигнал
    }
    s_timer_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_btn_cb(lv_event_t *e) {
    if (s_timer_running || s_remaining_sec <= 0) return;
    s_timer_running = true;
    xTaskCreate(timer_task, "timer", 4096, NULL, 5, &s_timer_task_handle);
}

static void pause_btn_cb(lv_event_t *e) {
    if (!s_timer_running) return;
    s_timer_running = false;
}

static void stop_btn_cb(lv_event_t *e) {
    s_timer_running = false;
    s_remaining_sec = 0;
    update_display();
}

static void inc_time_cb(lv_event_t *e) {
    if (s_timer_running) return;
    s_remaining_sec += 60; // Добавляем 1 минуту
    update_display();
}

lv_obj_t *timer_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    s_time_label = lv_label_create(s_screen);
    lv_label_set_text(s_time_label, "00:00");
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_label, &font_seg_115, 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -30);

    // Кнопка добавления времени (нажимаем на сами цифры или кнопку сверху)
    lv_obj_t *add_btn = lv_button_create(s_screen);
    lv_obj_set_size(add_btn, 120, 40);
    lv_obj_align(add_btn, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(add_btn, inc_time_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, "+1 мин");
    lv_obj_set_style_text_font(add_lbl, &font_ru_24, 0);
    lv_obj_center(add_lbl);

    // Кнопки управления внизу
    lv_obj_t *start_btn = lv_button_create(s_screen);
    lv_obj_set_size(start_btn, 80, 60);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_LEFT, 20, -60);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x008800), 0);
    lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(start_lbl);

    lv_obj_t *pause_btn = lv_button_create(s_screen);
    lv_obj_set_size(pause_btn, 80, 60);
    lv_obj_align(pause_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_bg_color(pause_btn, lv_color_hex(0xCC8800), 0);
    lv_obj_add_event_cb(pause_btn, pause_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pause_lbl = lv_label_create(pause_btn);
    lv_label_set_text(pause_lbl, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(pause_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(pause_lbl);

    lv_obj_t *stop_btn = lv_button_create(s_screen);
    lv_obj_set_size(stop_btn, 80, 60);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -60);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xCC0000), 0);
    lv_obj_add_event_cb(stop_btn, stop_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stop_lbl = lv_label_create(stop_btn);
    lv_label_set_text(stop_lbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(stop_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(stop_lbl);

    return s_screen;
}
lv_obj_t *timer_ui_get_screen(void) { return s_screen; }
