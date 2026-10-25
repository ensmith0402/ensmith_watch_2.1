#pragma once

#include "lvgl.h"

lv_obj_t *settings_ble_create(void);
lv_obj_t *settings_ble_get_screen(void);
void settings_ble_update_status(void);
