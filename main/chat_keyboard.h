#pragma once

#include "lvgl.h"

typedef void (*keyboard_key_cb_t)(const char *key);

lv_obj_t *chat_keyboard_create(lv_obj_t *parent, keyboard_key_cb_t cb);
void chat_keyboard_set_visible(bool visible);
void chat_keyboard_reset(void);
void chat_keyboard_redraw(void);
