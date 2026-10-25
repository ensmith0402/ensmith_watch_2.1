#pragma once

#include "lvgl.h"

/**
 * @brief Создать экран меню
 */
lv_obj_t *menu_ui_create(void);

/**
 * @brief Получить указатель на экран меню
 */
lv_obj_t *menu_ui_get_screen(void);
