#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Инициализация Wi-Fi и подключение к сети
 *
 * @param ssid     Имя сети
 * @param password Пароль
 * @return ESP_OK при успехе
 */
esp_err_t wifi_manager_init(const char *ssid, const char *password);

/**
 * @brief Проверка, подключены ли мы к Wi-Fi
 */
bool wifi_manager_is_connected(void);
