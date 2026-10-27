#pragma once
#include "lvgl.h"

lv_obj_t *speedtest_ui_create(void);
lv_obj_t *speedtest_ui_get_screen(void);
void speedtest_ui_update_speed(float speed_mbps);
lv_obj_t *speedtest_ui_create_speed_screen(void);
lv_obj_t *speedtest_ui_get_speed_screen(void);
