#include "recorder_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "RECORDER";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static bool s_mounted = false;

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void test_sd_card(void) {
    if (s_mounted) {
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "Уже смонтирована");
        bsp_display_unlock();
        return;
    }

    ESP_LOGI(TAG, "Попытка монтирования SD-карты через SDMMC (1-bit)...");
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Монтирование...");
    bsp_display_unlock();

    // Настройка SDMMC (1-bit режим)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT; // Важно: 1-bit режим
    host.max_freq_khz = 10000; // Снижаем частоту для надежности

    // Пины для SDMMC (из документации Waveshare)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 1-bit
    slot_config.clk = 2;   // SCK
    slot_config.cmd = 1;   // MOSI/CMD
    slot_config.d0 = 3;    // MISO/D0
    // slot_config.d1, d2, d3 не используем

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
    ESP_LOGI(TAG, "SD-карта успешно смонтирована!");
    
    // Пробуем записать тестовый файл
    FILE *f = fopen("/sdcard/test.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Не удалось создать файл");
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "Ошибка записи");
        bsp_display_unlock();
    } else {
        fprintf(f, "Hello from ESP32!");
        fclose(f);
        ESP_LOGI(TAG, "Файл test.txt создан!");
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "SD-карта ОК!");
        bsp_display_unlock();
    }
}

// Обертка для вызова из LVGL
static void test_sd_btn_cb(lv_event_t *e) {
    test_sd_card();
}

lv_obj_t *recorder_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Диктофон (Тест SD)");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_ru_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Нажмите для теста");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_status_label, &font_ru_24, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn = lv_button_create(s_screen);
    lv_obj_set_size(btn, 160, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x008800), 0);
    lv_obj_add_event_cb(btn, test_sd_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Тест SD");
    lv_obj_set_style_text_font(btn_lbl, &font_ru_24, 0);
    lv_obj_center(btn_lbl);

    return s_screen;
}
lv_obj_t *recorder_ui_get_screen(void) { return s_screen; }
