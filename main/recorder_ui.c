#include "recorder_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "RECORDER";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static bool s_mounted = false;
static bool s_recording = false;
static int s_file_counter = 0;
static TaskHandle_t s_record_task = NULL;
static esp_codec_dev_handle_t s_mic = NULL;
static FILE *s_wav_file = NULL;

#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define CHANNELS 1

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

// Функция для записи заголовка WAV-файла
static void write_wav_header(FILE *f, int data_size) {
    uint8_t header[44];
    int file_size = data_size + 36;
    
    // RIFF
    memcpy(header, "RIFF", 4);
    memcpy(header + 4, &file_size, 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    
    // fmt chunk
    int fmt_size = 16;
    memcpy(header + 16, &fmt_size, 4);
    short audio_format = 1; // PCM
    memcpy(header + 20, &audio_format, 2);
    short channels = CHANNELS;
    memcpy(header + 22, &channels, 2);
    int sample_rate = SAMPLE_RATE;
    memcpy(header + 24, &sample_rate, 4);
    int byte_rate = SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8;
    memcpy(header + 28, &byte_rate, 4);
    short block_align = CHANNELS * BITS_PER_SAMPLE / 8;
    memcpy(header + 32, &block_align, 2);
    short bits = BITS_PER_SAMPLE;
    memcpy(header + 34, &bits, 2);
    
    // data chunk
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &data_size, 4);
    
    fseek(f, 0, SEEK_SET);
    fwrite(header, 1, 44, f);
}

static void record_task(void *arg) {
    uint8_t *buffer = malloc(4096);
    int total_bytes = 0;
    
    ESP_LOGI(TAG, "Начинаем запись...");
    
    // Открываем файл (пока пишем заглушку в заголовке, потом обновим)
    char filename[32];
    snprintf(filename, sizeof(filename), "/sdcard/record%d.wav", s_file_counter++);
    s_wav_file = fopen(filename, "wb");
    if (!s_wav_file) {
        ESP_LOGE(TAG, "Не удалось создать файл");
        s_recording = false;
        free(buffer);
        vTaskDelete(NULL);
        return;
    }
    
    // Пишем пустой заголовок (обновим в конце)
    uint8_t empty_header[44] = {0};
    fwrite(empty_header, 1, 44, s_wav_file);
    
    while (s_recording) {
        if (s_mic) {
            esp_err_t ret = esp_codec_dev_read(s_mic, buffer, 4096);
            if (ret == ESP_OK) {
                fwrite(buffer, 1, 4096, s_wav_file);
                total_bytes += 4096;
            } else {
                ESP_LOGE(TAG, "Ошибка чтения микрофона: %s", esp_err_to_name(ret));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Обновляем заголовок с реальным размером
    write_wav_header(s_wav_file, total_bytes);
    fclose(s_wav_file);
    s_wav_file = NULL;
    
    free(buffer);
    ESP_LOGI(TAG, "Запись завершена. Всего байт: %d", total_bytes);
    s_record_task = NULL;
    vTaskDelete(NULL);
}

static void start_rec_btn_cb(lv_event_t *e) {
    if (!s_mounted) {
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "Сначала вставьте SD-карту!");
        bsp_display_unlock();
        return;
    }
    if (s_recording) return;
    
    s_recording = true;
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Идет запись...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFF0000), 0);
    bsp_display_unlock();
    
    xTaskCreate(record_task, "record", 8192, NULL, 5, &s_record_task);
}

static void stop_rec_btn_cb(lv_event_t *e) {
    if (!s_recording) return;
    s_recording = false;
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Сохранено");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
    bsp_display_unlock();
}

static void mount_sd_card(void) {
    if (s_mounted) return;
    
    ESP_LOGI(TAG, "Монтирование SD-карты...");
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Монтирование...");
    bsp_display_unlock();

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = 10000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = 2;
    slot_config.cmd = 1;
    slot_config.d0 = 3;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка монтирования: %s", esp_err_to_name(ret));
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "Карта не найдена");
        bsp_display_unlock();
        return;
    }
    
    s_mounted = true;
    ESP_LOGI(TAG, "SD-карта ОК");
}

// Функция инициализации микрофона (вызываем один раз при старте)
static void init_mic(void) {
    esp_err_t ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка bsp_audio_init: %s", esp_err_to_name(ret));
        return;
    }
    
    s_mic = bsp_audio_codec_microphone_init();
    if (!s_mic) {
        ESP_LOGE(TAG, "Ошибка инициализации микрофона");
        return;
    }
    
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel = CHANNELS,
        .sample_rate = SAMPLE_RATE,
    };
    esp_codec_dev_open(s_mic, &fs);
    esp_codec_dev_set_in_gain(s_mic, 37.5); // Максимальный Gain
    ESP_LOGI(TAG, "Микрофон инициализирован");
}

lv_obj_t *recorder_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Диктофон");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Инициализация...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_24, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, -20);

    // Кнопки
    lv_obj_t *start_btn = lv_button_create(s_screen);
    lv_obj_set_size(start_btn, 120, 60);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_LEFT, 30, -40);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xCC0000), 0);
    lv_obj_add_event_cb(start_btn, start_rec_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, LV_SYMBOL_VIDEO); // Используем иконку камеры (как запись)
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(start_lbl);

    lv_obj_t *stop_btn = lv_button_create(s_screen);
    lv_obj_set_size(stop_btn, 120, 60);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -40);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(stop_btn, stop_rec_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stop_lbl = lv_label_create(stop_btn);
    lv_label_set_text(stop_lbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(stop_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(stop_lbl);

    // Монтируем карту и инициализируем микрофон
    mount_sd_card();
    init_mic();
    
    if (s_mounted) {
        lv_label_set_text(s_status_label, "Готов к записи");
    }

    return s_screen;
}
lv_obj_t *recorder_ui_get_screen(void) { return s_screen; }
