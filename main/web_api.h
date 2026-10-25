#pragma once

#include "esp_err.h"

/**
 * @brief Зарегистрировать наши HTTP endpoints на сервере wifi_cfg
 */
esp_err_t web_api_init(void);
