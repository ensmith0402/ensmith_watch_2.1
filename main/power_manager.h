#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Инициализация менеджера питания
 */
esp_err_t power_manager_init(void);

/**
 * @brief Запуск задачи энергосбережения
 */
void power_manager_start(void);

/**
 * @brief Сбросить таймер бездействия (вызывается при таче)
 */
void power_manager_reset_timer(void);

/**
 * @brief Экран включён?
 */
bool power_manager_is_screen_on(void);
