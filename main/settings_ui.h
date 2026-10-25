#pragma once

#include "lvgl.h"

/**
 * @brief Создать главный экран настроек
 */
lv_obj_t *settings_ui_create(void);

/**
 * @brief Получить указатель на экран
 */
lv_obj_t *settings_ui_get_screen(void);
