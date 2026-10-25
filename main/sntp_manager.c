#include "sntp_manager.h"
#include "rtc_manager.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <string.h>

static const char *TAG = "SNTP";

#define SNTP_SYNCED_BIT BIT0

static EventGroupHandle_t s_sntp_event_group = NULL;
static bool s_synced = false;

static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP: время получено");
    if (s_sntp_event_group) {
        xEventGroupSetBits(s_sntp_event_group, SNTP_SYNCED_BIT);
    }
}

esp_err_t sntp_manager_sync(int timeout_sec)
{
    if (s_synced) {
        ESP_LOGI(TAG, "SNTP: уже синхронизировано");
        return ESP_OK;
    }

    s_sntp_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "SNTP: инициализация...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP: ожидание ответа (до %d сек)...", timeout_sec);
    EventBits_t bits = xEventGroupWaitBits(s_sntp_event_group,
        SNTP_SYNCED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_sec * 1000));

    if (!(bits & SNTP_SYNCED_BIT)) {
        ESP_LOGE(TAG, "SNTP: таймаут, время не получено");
        return ESP_FAIL;
    }

    // Ждём немного, чтобы система обновила время
    vTaskDelay(pdMS_TO_TICKS(500));

    // Читаем системное время
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "SNTP: время всё ещё невалидно");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SNTP: системное время: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Записываем в RTC
    rtc_time_t rtc_time = {
        .year = timeinfo.tm_year + 1900,
        .month = timeinfo.tm_mon + 1,
        .day = timeinfo.tm_mday,
        .hour = timeinfo.tm_hour,
        .minute = timeinfo.tm_min,
        .second = timeinfo.tm_sec,
        .weekday = timeinfo.tm_wday,
    };

    esp_err_t ret = rtc_manager_set_time(&rtc_time);
    if (ret == ESP_OK) {
        s_synced = true;
        ESP_LOGI(TAG, "SNTP: время записано в RTC");
    } else {
        ESP_LOGE(TAG, "SNTP: не удалось записать в RTC: %s", esp_err_to_name(ret));
    }

    return ret;
}

bool sntp_manager_is_synced(void)
{
    return s_synced;
}
