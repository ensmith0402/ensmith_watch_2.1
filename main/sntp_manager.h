#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Запустить SNTP-синхронизацию
 *
 * @param timeout_sec Сколько секунд ждать ответа от сервера
 * @return ESP_OK если время получено и записано в RTC
 */
esp_err_t sntp_manager_sync(int timeout_sec);

/**
 * @brief Проверить, синхронизировано ли время
 */
bool sntp_manager_is_synced(void);
