#pragma once

#include "esp_err.h"

/**
 * @brief Установить API-ключ DeepSeek
 */
void deepseek_set_api_key(const char *api_key);

/**
 * @brief Отправить текстовый запрос к DeepSeek
 *
 * @param prompt        Текст запроса
 * @param out_response  Указатель на строку с ответом (нужно free())
 * @return ESP_OK при успехе
 */
esp_err_t deepseek_send(const char *prompt, char **out_response);
