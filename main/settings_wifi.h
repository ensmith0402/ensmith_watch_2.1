#pragma once

#include "lvgl.h"

/**
 * @brief Создать экран настройки Wi-Fi
 */
lv_obj_t *settings_wifi_create(void);

/**
 * @brief Получить указатель на экран
 */
lv_obj_t *settings_wifi_get_screen(void);
