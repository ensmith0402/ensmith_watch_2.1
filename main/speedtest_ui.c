#include "speedtest_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include <string.h>

LV_FONT_DECLARE(font_ru_24);
LV_FONT_DECLARE(font_ru_48);

static const char *TAG = "NET_TEST";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_ping1_label = NULL;
static lv_obj_t *s_ping2_label = NULL;
static lv_obj_t *s_status_label = NULL;

static bool s_is_testing = false;
static int s_success_count = 0;
static int64_t s_total_time_ms = 0;
static int s_pings_done = 0;

static void speedtest_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → часы");
        screen_manager_switch(SCREEN_WATCH);
    } else if (dir == LV_DIR_LEFT) {
        ESP_LOGI(TAG, "Свайп влево → проверка скорости");
        screen_manager_switch(SCREEN_NET_SPEED);
    }
}

// Колбэки для пинга
static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    
    ESP_LOGI(TAG, "Пинг успешен: %d байт, ttl=%d, время=%lu ms", recv_len, ttl, elapsed_time);
    
    s_success_count++;
    s_total_time_ms += elapsed_time;
    s_pings_done++;
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "Пинг таймаут");
    s_pings_done++;
}

static void on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    
    ESP_LOGI(TAG, "Пинг завершен: отправлено %lu, получено %lu", transmitted, received);
}

static void perform_ping(const char *ip_str, int count)
{
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    
    if (!ipaddr_aton(ip_str, &target_addr)) {
        ESP_LOGE(TAG, "Неверный IP: %s", ip_str);
        return;
    }
    
    s_success_count = 0;
    s_total_time_ms = 0;
    s_pings_done = 0;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = count;
    ping_config.timeout_ms = 1000; // Ждем ответ 1 секунду
    ping_config.interval_ms = 500;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = on_ping_end,
    };

    esp_ping_handle_t ping;
    esp_err_t err = esp_ping_new_session(&ping_config, &cbs, &ping);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка создания сессии пинга: %s", esp_err_to_name(err));
        return;
    }

    esp_ping_start(ping);
    
    // Ждем завершения всех пингов
    while (s_pings_done < count) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
}

static void ping_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Запуск пинг-теста...");
    
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Пинг 8.8.8.8...");
    lv_label_set_text(s_ping1_label, "8.8.8.8: ожидание...");
    lv_label_set_text(s_ping2_label, "1.1.1.1: ожидание...");
    bsp_display_unlock();

    // Тест 1: 8.8.8.8 (Google DNS)
    perform_ping("8.8.8.8", 4);
    
    bsp_display_lock(0);
    if (s_success_count > 0) {
        float avg = (float)s_total_time_ms / s_success_count;
        char buf[64];
        snprintf(buf, sizeof(buf), "8.8.8.8: %.1f мс (%d/4)", avg, s_success_count);
        lv_label_set_text(s_ping1_label, buf);
        lv_obj_set_style_text_color(s_ping1_label, lv_color_hex(0x00FF00), 0); // Зеленый
    } else {
        lv_label_set_text(s_ping1_label, "8.8.8.8: недоступен");
        lv_obj_set_style_text_color(s_ping1_label, lv_color_hex(0xFF0000), 0); // Красный
    }
    lv_label_set_text(s_status_label, "Пинг 1.1.1.1...");
    bsp_display_unlock();

    // Тест 2: 1.1.1.1 (Cloudflare DNS)
    perform_ping("1.1.1.1", 4);
    
    bsp_display_lock(0);
    if (s_success_count > 0) {
        float avg = (float)s_total_time_ms / s_success_count;
        char buf[64];
        snprintf(buf, sizeof(buf), "1.1.1.1: %.1f мс (%d/4)", avg, s_success_count);
        lv_label_set_text(s_ping2_label, buf);
        lv_obj_set_style_text_color(s_ping2_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(s_ping2_label, "1.1.1.1: недоступен");
        lv_obj_set_style_text_color(s_ping2_label, lv_color_hex(0xFF0000), 0);
    }
    
    if (s_success_count > 0) {
        lv_label_set_text(s_status_label, "Готово");
    } else {
        lv_label_set_text(s_status_label, "Нет сети / блокировка ICMP");
    }
    bsp_display_unlock();

    s_is_testing = false;
    vTaskDelete(NULL);
}

static void start_btn_cb(lv_event_t *e)
{
    if (s_is_testing) return;
    s_is_testing = true;
    xTaskCreate(ping_task, "ping_task", 8192, NULL, 5, NULL);
}

lv_obj_t *speedtest_ui_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_add_event_cb(s_screen, speedtest_gesture_cb, LV_EVENT_GESTURE, NULL);

    s_title = lv_label_create(s_screen);
    lv_label_set_text(s_title, "Диагностика сети");
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_title, &font_ru_24, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 20);

    // Метка для 8.8.8.8
    s_ping1_label = lv_label_create(s_screen);
    lv_label_set_text(s_ping1_label, "8.8.8.8: ---");
    lv_obj_set_style_text_color(s_ping1_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_ping1_label, &font_ru_24, 0);
    lv_obj_align(s_ping1_label, LV_ALIGN_TOP_MID, 0, 100);

    // Метка для 1.1.1.1
    s_ping2_label = lv_label_create(s_screen);
    lv_label_set_text(s_ping2_label, "1.1.1.1: ---");
    lv_obj_set_style_text_color(s_ping2_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_ping2_label, &font_ru_24, 0);
    lv_obj_align(s_ping2_label, LV_ALIGN_TOP_MID, 0, 160);

    // Статус
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Нажмите 'Пуск'");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_24, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 0);

    // Кнопка ПУСК
    lv_obj_t *start_btn = lv_button_create(s_screen);
    lv_obj_set_size(start_btn, 160, 60);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x008800), 0);
    lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, "Пуск");
    lv_obj_set_style_text_font(start_lbl, &font_ru_24, 0);
    lv_obj_center(start_lbl);

    ESP_LOGI(TAG, "Экран диагностики сети создан");
    return s_screen;
}

lv_obj_t *speedtest_ui_get_screen(void)
{
    return s_screen;
}

void speedtest_ui_update_speed(float speed_mbps) {
    // Заглушка, чтобы не ломать старые вызовы, если они остались
}

// ============================================================
// Экран проверки скорости интернета (скачивание ~1 МБ)
// ============================================================

#define SPEEDTEST_URL "http://kichesova.ru/test.bin"
#define SPEEDTEST_MAX_BYTES (1024 * 1024) // 1 МБ

static lv_obj_t *s_speed_screen = NULL;
static lv_obj_t *s_speed_title = NULL;
static lv_obj_t *s_speed_value = NULL;
static lv_obj_t *s_speed_status = NULL;
static lv_obj_t *s_speed_start_btn = NULL;
static lv_obj_t *s_speed_reset_btn = NULL;
static lv_obj_t *s_speed_start_lbl = NULL;
static lv_obj_t *s_speed_reset_lbl = NULL;
static bool s_is_speed_testing = false;
static size_t s_speed_bytes = 0;
static int64_t s_speed_start_us = 0;
static int64_t s_speed_end_us = 0;
static float s_speed_mbps = 0;

static void speed_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Свайп вправо → пинг-тест");
        screen_manager_switch(SCREEN_SPEEDTEST);
    } else if (dir == LV_DIR_LEFT) {
        ESP_LOGI(TAG, "Свайп влево → часы");
        screen_manager_switch(SCREEN_WATCH);
    }
}

static esp_err_t speed_http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (s_speed_bytes < SPEEDTEST_MAX_BYTES) {
            s_speed_bytes += evt->data_len;
            if (s_speed_bytes > (size_t)SPEEDTEST_MAX_BYTES) {
                s_speed_bytes = SPEEDTEST_MAX_BYTES;
            }
        }
    }
    return ESP_OK;
}

static void speed_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Запуск теста скорости...");

    bsp_display_lock(0);
    lv_label_set_text(s_speed_status, "Загрузка 1 МБ...");
    lv_obj_set_style_text_color(s_speed_status, lv_color_hex(0xCCCCCC), 0);
    bsp_display_unlock();

    s_speed_bytes = 0;
    s_speed_mbps = 0;
    s_speed_start_us = esp_timer_get_time();

    esp_http_client_config_t config = {
        .url = SPEEDTEST_URL,
        .method = HTTP_METHOD_GET,
        .event_handler = speed_http_event_handler,
        .timeout_ms = 30000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // Запрашиваем только первые 1 МБ файла (Range: bytes=0-1048575)
    esp_http_client_set_header(client, "Range", "bytes=0-1048575");
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    s_speed_end_us = esp_timer_get_time();
    int64_t elapsed_us = s_speed_end_us - s_speed_start_us;
    float elapsed_s = (float)elapsed_us / 1000000.0f;

    if (err == ESP_OK && elapsed_s > 0.0f && s_speed_bytes > 0) {
        s_speed_mbps = ((float)s_speed_bytes * 8.0f) / elapsed_s / 1000000.0f; // Мбит/с
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f Мбит/с", s_speed_mbps);
    bsp_display_lock(0);
    lv_label_set_text(s_speed_value, buf);
    lv_obj_set_style_text_color(s_speed_value, lv_color_hex(0x00FF00), 0);
    if (err == ESP_OK && s_speed_bytes > 0) {
        lv_label_set_text(s_speed_status, "Готово");
    } else {
        lv_label_set_text(s_speed_status, "Скачивание не удалось");
    }
    bsp_display_unlock();

    s_is_speed_testing = false;
    vTaskDelete(NULL);
}

static void speed_start_btn_cb(lv_event_t *e)
{
    if (s_is_speed_testing) return;
    s_is_speed_testing = true;
    s_speed_bytes = 0;
    s_speed_mbps = 0;
    bsp_display_lock(0);
    lv_label_set_text(s_speed_status, "Загрузка 1 МБ...");
    bsp_display_unlock();
    xTaskCreate(speed_test_task, "speed_test", 8192, NULL, 5, NULL);
}

static void speed_reset_btn_cb(lv_event_t *e)
{
    s_is_speed_testing = false;
    s_speed_bytes = 0;
    s_speed_mbps = 0;
    bsp_display_lock(0);
    lv_label_set_text(s_speed_status, "Нажмите 'Пуск'");
    bsp_display_unlock();
}

lv_obj_t *speedtest_ui_create_speed_screen(void)
{
    s_speed_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_speed_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_speed_screen, 0, 0);
    lv_obj_set_style_pad_all(s_speed_screen, 0, 0);

    lv_obj_add_event_cb(s_speed_screen, speed_gesture_cb, LV_EVENT_GESTURE, NULL);

    s_speed_title = lv_label_create(s_speed_screen);
    lv_label_set_text(s_speed_title, "Скорость сети");
    lv_obj_set_style_text_color(s_speed_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_speed_title, &font_ru_24, 0);
    lv_obj_align(s_speed_title, LV_ALIGN_TOP_MID, 0, 20);

    s_speed_value = lv_label_create(s_speed_screen);
    lv_label_set_text(s_speed_value, "0.00 Мбит/с");
    lv_obj_set_style_text_color(s_speed_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_speed_value, &font_ru_48, 0);
    lv_obj_align(s_speed_value, LV_ALIGN_CENTER, 0, -30);

    s_speed_status = lv_label_create(s_speed_screen);
    lv_label_set_text(s_speed_status, "Нажмите 'Пуск'");
    lv_obj_set_style_text_color(s_speed_status, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_speed_status, &font_ru_24, 0);
    lv_obj_align(s_speed_status, LV_ALIGN_CENTER, 0, 60);

    s_speed_start_btn = lv_button_create(s_speed_screen);
    lv_obj_set_size(s_speed_start_btn, 160, 60);
    lv_obj_align(s_speed_start_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(s_speed_start_btn, lv_color_hex(0x008800), 0);
    lv_obj_add_event_cb(s_speed_start_btn, speed_start_btn_cb, LV_EVENT_CLICKED, NULL);
    s_speed_start_lbl = lv_label_create(s_speed_start_btn);
    lv_label_set_text(s_speed_start_lbl, "Пуск");
    lv_obj_set_style_text_font(s_speed_start_lbl, &font_ru_24, 0);
    lv_obj_center(s_speed_start_lbl);

    s_speed_reset_btn = lv_button_create(s_speed_screen);
    lv_obj_set_size(s_speed_reset_btn, 160, 60);
    lv_obj_align(s_speed_reset_btn, LV_ALIGN_BOTTOM_MID, 0, -110);
    lv_obj_set_style_bg_color(s_speed_reset_btn, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(s_speed_reset_btn, speed_reset_btn_cb, LV_EVENT_CLICKED, NULL);
    s_speed_reset_lbl = lv_label_create(s_speed_reset_btn);
    lv_label_set_text(s_speed_reset_lbl, "Сброс");
    lv_obj_set_style_text_font(s_speed_reset_lbl, &font_ru_24, 0);
    lv_obj_center(s_speed_reset_lbl);

    ESP_LOGI(TAG, "Экран проверки скорости создан");
    return s_speed_screen;
}

lv_obj_t *speedtest_ui_get_speed_screen(void)
{
    return s_speed_screen;
}
