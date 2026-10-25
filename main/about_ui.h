#pragma once

#include "lvgl.h"

/**
 * @brief Создать экран "О часах"
 */
lv_obj_t *about_ui_create(void);

/**
 * @brief Получить указатель на экран
 */
lv_obj_t *about_ui_get_screen(void);

/**
 * @brief Обновить данные (вызывать при показе экрана)
 */
void about_ui_update(void);
