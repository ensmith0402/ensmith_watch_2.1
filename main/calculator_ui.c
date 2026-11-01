#include "calculator_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_32);
LV_FONT_DECLARE(font_ru_48);

static const char *TAG = "CALC_UI";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_result_label = NULL;
static char s_current_input[32] = "";
static char s_operator = 0;
static float s_first_operand = 0;

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void update_display(void) {
    if (strlen(s_current_input) == 0) {
        lv_label_set_text(s_result_label, "0");
    } else {
        lv_label_set_text(s_result_label, s_current_input);
    }
}

static void num_btn_cb(lv_event_t *e) {
    const char *txt = lv_label_get_text(lv_obj_get_child(lv_event_get_target(e), 0));
    if (strlen(s_current_input) < 15) {
        strcat(s_current_input, txt);
        update_display();
    }
}

static void op_btn_cb(lv_event_t *e) {
    const char *txt = lv_label_get_text(lv_obj_get_child(lv_event_get_target(e), 0));
    
    if (strcmp(txt, "C") == 0) {
        s_current_input[0] = '\0';
        s_first_operand = 0;
        s_operator = 0;
        update_display();
        return;
    }
    
    if (strcmp(txt, "=") == 0) {
        if (s_operator != 0 && strlen(s_current_input) > 0) {
            float second_operand = atof(s_current_input);
            float result = 0;
            if (s_operator == '+') result = s_first_operand + second_operand;
            else if (s_operator == '-') result = s_first_operand - second_operand;
            else if (s_operator == 'x') result = s_first_operand * second_operand;
            else if (s_operator == '/') {
                if (second_operand != 0) result = s_first_operand / second_operand;
                else {
                    lv_label_set_text(s_result_label, "Error");
                    s_current_input[0] = '\0';
                    return;
                }
            }
            snprintf(s_current_input, sizeof(s_current_input), "%.2f", result);
            char *dot = strchr(s_current_input, '.');
            if (dot) {
                char *end = s_current_input + strlen(s_current_input) - 1;
                while (end > dot && *end == '0') { *end = '\0'; end--; }
                if (end == dot) *end = '\0';
            }
            s_operator = 0;
            update_display();
        }
        return;
    }

    if (strlen(s_current_input) > 0) {
        s_first_operand = atof(s_current_input);
        s_operator = txt[0];
        s_current_input[0] = '\0';
    }
}

lv_obj_t *calculator_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    s_result_label = lv_label_create(s_screen);
    lv_label_set_text(s_result_label, "0");
    lv_obj_set_style_text_color(s_result_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_result_label, &font_ru_48, 0);
    lv_obj_align(s_result_label, LV_ALIGN_TOP_RIGHT, -20, 60);
    lv_obj_set_width(s_result_label, 380);
    lv_obj_set_style_text_align(s_result_label, LV_TEXT_ALIGN_RIGHT, 0);

    const char *btns[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "x"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            lv_obj_t *btn = lv_button_create(s_screen);
            lv_obj_set_size(btn, 85, 65);
            lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 15 + col * 95, -20 - (3 - row) * 75);
            
            if (strcmp(btns[row][col], "=") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x008800), 0);
            } else if (strchr("+-x/", btns[row][col][0]) || strcmp(btns[row][col], "C") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), 0);
            } else {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
            }

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, btns[row][col]);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
            lv_obj_set_style_text_font(lbl, &font_ru_24, 0);
            lv_obj_center(lbl);
            lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

            if (strchr("0123456789", btns[row][col][0])) {
                lv_obj_add_event_cb(btn, num_btn_cb, LV_EVENT_CLICKED, NULL);
            } else {
                lv_obj_add_event_cb(btn, op_btn_cb, LV_EVENT_CLICKED, NULL);
            }
        }
    }

    return s_screen;
}

lv_obj_t *calculator_ui_get_screen(void) {
    return s_screen;
}
