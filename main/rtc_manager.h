#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    int year;      // Полный год, например, 2026
    int month;     // 1-12
    int day;       // 1-31
    int hour;      // 0-23
    int minute;    // 0-59
    int second;    // 0-59
    int weekday;   // 0-6 (0=воскресенье)
} rtc_time_t;

/**
 * @brief Инициализировать RTC (проверить, идёт ли время)
 * @return ESP_OK если время валидно, ESP_FAIL если нужно установить
 */
esp_err_t rtc_manager_init(void);

/**
 * @brief Прочитать текущее время из RTC
 */
esp_err_t rtc_manager_get_time(rtc_time_t *out_time);

/**
 * @brief Установить время в RTC
 */
esp_err_t rtc_manager_set_time(const rtc_time_t *time);
