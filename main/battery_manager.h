#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    int  level;         // 0-100 (%)
    int  voltage_mv;    // Напряжение в мВ
    bool charging;      // Идёт зарядка
    bool full;          // Полностью заряжена
    bool discharging;   // Разряжается
} battery_info_t;

/**
 * @brief Инициализировать модуль батареи (добавить AXP2101 в I2C)
 */
esp_err_t battery_manager_init(void);

/**
 * @brief Прочитать текущее состояние батареи
 */
esp_err_t battery_manager_get(battery_info_t *out_info);

/**
 * @brief Получить только уровень заряда (0-100)
 */
int battery_manager_get_level(void);

/**
 * @brief Идёт ли зарядка
 */
bool battery_manager_is_charging(void);
