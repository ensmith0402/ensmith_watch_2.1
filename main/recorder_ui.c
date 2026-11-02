#include "recorder_ui.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include <stdio.h>

LV_FONT_DECLARE(font_ru_24);

static const char *TAG = "RECORDER";
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static bool s_spi_initialized = false;

static void gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        screen_manager_switch(SCREEN_MENU);
    }
}

static void test_sd_card(void) {
    ESP_LOGI(TAG, "Попытка монтирования SD-карты...");
    bsp_display_lock(0);
    lv_label_set_text(s_status_label, "Монтирование...");
    bsp_display_unlock();

    // Настройка SPI для SD-карты (пины взяты из документации Waveshare)
    gpio_set_pull_mode(1, GPIO_PULLUP_ONLY); // MOSI
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY); // MISO
    gpio_set_pull_mode(3, GPIO_PULLUP_ONLY); // SCK

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 1,
        .miso_io_num = 2,
        .sclk_io_num = 3,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = ESP_OK;
    if (!s_spi_initialized) {
        ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Ошибка инициализации SPI: %s", esp_err_to_name(ret));
            bsp_display_lock(0);
            lv_label_set_text(s_status_label, "Ошибка SPI");
            bsp_display_unlock();
            return;
        }
        s_spi_initialized = true;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;
    host.slot = SPI3_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 17;
    slot_config.host_id = SPI3_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    vTaskDelay(pdMS_TO_TICKS(500)); // Небольшая пауза
    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка монтирования: %s", esp_err_to_name(ret));
        bsp_display_lock(0);
        lv_label_set_text(s_status_label, "Карта не найдена");
        bsp_display_unlock();
        return;
    }

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

lv_obj_t *recorder_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Диктофон (Тест)");
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
    lv_obj_add_event_cb(btn, (lv_event_cb_t)test_sd_card, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Тест SD");
    lv_obj_set_style_text_font(btn_lbl, &font_ru_24, 0);
    lv_obj_center(btn_lbl);

    return s_screen;
}
lv_obj_t *recorder_ui_get_screen(void) { return s_screen; }
