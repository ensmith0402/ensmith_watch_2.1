#include "power_manager.h"
#include "config_manager.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "POWER";

#define QMI8658_ADDR        0x6B
#define QMI8658_WHO_AM_I    0x00
#define QMI8658_CTRL1       0x02
#define QMI8658_CTRL2       0x03
#define QMI8658_CTRL3       0x04
#define QMI8658_CTRL5       0x06
#define QMI8658_CTRL7       0x08
#define QMI8658_AX_L        0x35

#define INACTIVITY_SEC      60
#define TILT_THRESHOLD_Z    0.4f
#define TILT_THRESHOLD_Y    0.5f

static i2c_master_dev_handle_t s_imu_dev = NULL;
static bool s_screen_on = true;
static int s_inactivity_counter = 0;
static TaskHandle_t s_power_task = NULL;

static esp_err_t imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_imu_dev, buf, 2, 1000);
}

static esp_err_t imu_read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_imu_dev, &reg, 1, buf, len, 1000);
}

static esp_err_t imu_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_FAIL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_imu_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add QMI8658: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t who = 0;
    ret = imu_read_regs(QMI8658_WHO_AM_I, &who, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 not responding");
        return ret;
    }
    ESP_LOGI(TAG, "QMI8658 WHO_AM_I: 0x%02X", who);

    imu_write_reg(QMI8658_CTRL1, 0x60);
    imu_write_reg(QMI8658_CTRL2, 0x14);
    imu_write_reg(QMI8658_CTRL3, 0x54);
    imu_write_reg(QMI8658_CTRL5, 0x11);
    imu_write_reg(QMI8658_CTRL7, 0x03);

    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "QMI8658 initialized");
    return ESP_OK;
}

static esp_err_t imu_read_accel(float *ax, float *ay, float *az)
{
    uint8_t buf[6];
    esp_err_t ret = imu_read_regs(QMI8658_AX_L, buf, 6);
    if (ret != ESP_OK) return ret;

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    *ax = raw_x / 8192.0f;
    *ay = raw_y / 8192.0f;
    *az = raw_z / 8192.0f;

    return ESP_OK;
}

/* ---------- Гашение / включение экрана ---------- */
static void screen_off(void)
{
    if (!s_screen_on) return;
    bsp_display_brightness_set(0);
    s_screen_on = false;
    ESP_LOGI(TAG, "Экран выключен");
}

static void screen_on(void)
{
    if (s_screen_on) return;
    const app_config_t *cfg = config_get();
    bsp_display_brightness_set(cfg->brightness);
    s_screen_on = true;
    s_inactivity_counter = 0;
    ESP_LOGI(TAG, "Экран включён");
}

/* ---------- Задача энергосбережения ---------- */
static void power_task(void *arg)
{
    int tick = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));
        tick++;

        if (s_screen_on) {
            if (tick % 5 == 0) {
                s_inactivity_counter++;
                if (s_inactivity_counter >= INACTIVITY_SEC) {
                    screen_off();
                    s_inactivity_counter = 0;
                }
            }
        }

        if (!s_screen_on) {
            float ax, ay, az;
            if (imu_read_accel(&ax, &ay, &az) == ESP_OK) {
                bool tilt_up = (az > TILT_THRESHOLD_Z);
                bool tilt_side = (fabsf(ay) > TILT_THRESHOLD_Y);

                if (tilt_up || tilt_side) {
                    screen_on();
                }
            }
        }
    }
}

/* ---------- Публичные функции ---------- */
esp_err_t power_manager_init(void)
{
    return imu_init();
}

void power_manager_start(void)
{
    xTaskCreate(power_task, "power", 4096, NULL, 2, &s_power_task);
}

void power_manager_reset_timer(void)
{
    s_inactivity_counter = 0;
}

bool power_manager_is_screen_on(void)
{
    return s_screen_on;
}
