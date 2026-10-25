#pragma once

#include "lvgl.h"

typedef void (*chat_send_cb_t)(const char *text);

void chat_ui_create(void);
void chat_ui_set_send_callback(chat_send_cb_t cb);
void chat_ui_set_response(const char *text);
void chat_ui_set_status(const char *text);
const char *chat_ui_get_input(void);
void chat_ui_clear_input(void);
lv_obj_t *chat_ui_get_screen(void);
