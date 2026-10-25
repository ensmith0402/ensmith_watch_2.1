#include "rtc_manager.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "bsp/esp-bsp.h"
#include <time.h>

static const char *TAG = "RTC";

#define PCF85063_ADDR       0x51
#define REG_SECONDS         0x04
#define REG_MINUTES         0x05
#define REG_HOURS           0x06
#define REG_DAYS            0x07
#define REG_WEEKDAYS        0x08
#define REG_MONTHS          0x09
#define REG_YEARS           0x0A

static i2c_master_dev_handle_t s_rtc_dev = NULL;

// Преобразование BCD -> DEC
static int bcd_to_dec(uint8_t val) {
    return ((val >> 4) & 0x0F) * 10 + (val & 0x0F);
}

// Преобразование DEC -> BCD
static uint8_t dec_to_bcd(int val) {
    return ((val / 10) << 4) | (val % 10);
}

// Чтение одного регистра
static esp_err_t read_reg(uint8_t reg, uint8_t *out_val) {
    return i2c_master_transmit_receive(s_rtc_dev, &reg, 1, out_val, 1, 1000);
}

// Запись одного регистра
static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_rtc_dev, buf, 2, 1000);
}

esp_err_t rtc_manager_init(void) {
    // I2C шина уже инициализирована BSP
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_FAIL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_ADDR,
        .scl_speed_hz = 100000, // 100 кГц для надёжности
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_rtc_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add RTC device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверяем VL-бит (Voltage Low) в регистре секунд
    uint8_t sec_val;
    ret = read_reg(REG_SECONDS, &sec_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read RTC");
        return ret;
    }

    if (sec_val & 0x80) {
        ESP_LOGW(TAG, "RTC: VL bit is set (voltage low / first use). Time is invalid.");
        return ESP_FAIL; // Сигнал, что нужно установить время
    }

    ESP_LOGI(TAG, "RTC initialized, time is valid");
    return ESP_OK;
}

esp_err_t rtc_manager_get_time(rtc_time_t *out_time) {
    uint8_t data[7];
    uint8_t reg = REG_SECONDS;

    // Читаем 7 байт подряд (сек, мин, час, день, день недели, месяц, год)
    esp_err_t ret = i2c_master_transmit_receive(s_rtc_dev, &reg, 1, data, 7, 1000);
    if (ret != ESP_OK) return ret;

    out_time->second = bcd_to_dec(data[0] & 0x7F); // Игнорируем VL-бит
    out_time->minute = bcd_to_dec(data[1] & 0x7F);
    out_time->hour   = bcd_to_dec(data[2] & 0x3F); // 24-часовой формат
    out_time->day    = bcd_to_dec(data[3] & 0x3F);
    out_time->weekday= bcd_to_dec(data[4] & 0x07);
    out_time->month  = bcd_to_dec(data[5] & 0x1F); // Игнорируем Century-бит

    // Год: если Century-бит (bit 7 регистра месяца) == 1, то это 1900-е, иначе 2000-е
    int year_2digit = bcd_to_dec(data[6]);
    if (data[5] & 0x80) {
        out_time->year = 1900 + year_2digit;
    } else {
        out_time->year = 2000 + year_2digit;
    }

    return ESP_OK;
}

esp_err_t rtc_manager_set_time(const rtc_time_t *time) {
    uint8_t data[7];

    data[0] = dec_to_bcd(time->second);
    data[1] = dec_to_bcd(time->minute);
    data[2] = dec_to_bcd(time->hour); // 24-часовой формат
    data[3] = dec_to_bcd(time->day);
    data[4] = dec_to_bcd(time->weekday);
    data[5] = dec_to_bcd(time->month);
    data[6] = dec_to_bcd(time->year % 100);

    // Управление Century-битом
    if (time->year < 2000) {
        data[5] |= 0x80; // 1900-е
    }
    // Для 2000-х оставляем 0

    // Сброс VL-бита: записываем 0 в старший бит секунд, чтобы показать, что время валидно
    data[0] &= 0x7F;

    uint8_t reg = REG_SECONDS;
    uint8_t buf[8] = { reg };
    for (int i = 0; i < 7; i++) {
        buf[i + 1] = data[i];
    }

    esp_err_t ret = i2c_master_transmit(s_rtc_dev, buf, 8, 1000);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTC time set: %04d-%02d-%02d %02d:%02d:%02d",
                 time->year, time->month, time->day,
                 time->hour, time->minute, time->second);
    }
    return ret;
}
