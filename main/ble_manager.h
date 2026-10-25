#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Инициализировать BLE GATT-сервер
 */
esp_err_t ble_manager_init(void);

/**
 * @brief Подключён ли клиент
 */
bool ble_manager_is_connected(void);

/**
 * @brief Отправить уведомление клиенту (если подключён)
 */
esp_err_t ble_manager_notify(const char *text);
