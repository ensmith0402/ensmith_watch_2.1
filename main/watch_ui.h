#pragma once

#include "lvgl.h"
#include "rtc_manager.h"
#include <stdbool.h>

/**
 * @brief Создать экран часов
 */
lv_obj_t *watch_ui_create(void);

/**
 * @brief Обновить отображение времени
 */
void watch_ui_update(const rtc_time_t *time);

/**
 * @brief Обновить иконку батареи
 * @param level   0-100 (или -1 если неизвестно)
 * @param charging Идёт ли зарядка
 */
void watch_ui_set_battery(int level, bool charging);

/**
 * @brief Обновить иконку Wi-Fi
 */
void watch_ui_set_wifi(bool connected);

/**
 * @brief Получить указатель на экран часов
 */
lv_obj_t *watch_ui_get_screen(void);
