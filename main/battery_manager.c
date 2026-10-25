#include "battery_manager.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "bsp/esp-bsp.h"
#include <string.h>

static const char *TAG = "BATTERY";

#define AXP2101_ADDR        0x34

#define REG_STATUS1         0x00    // Power Status
#define REG_STATUS2         0x01    // Charge Status
#define REG_BAT_VOLT_H      0x34
#define REG_BAT_VOLT_L      0x35
#define REG_BAT_PERCENT     0xA4

static i2c_master_dev_handle_t s_axp_dev = NULL;
static bool s_initialized = false;

static esp_err_t read_reg(uint8_t reg, uint8_t *out_val)
{
    return i2c_master_transmit_receive(s_axp_dev, &reg, 1, out_val, 1, 1000);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *out_buf, size_t len)
{
    return i2c_master_transmit_receive(s_axp_dev, &reg, 1, out_buf, len, 1000);
}

esp_err_t battery_manager_init(void)
{
    if (s_initialized) return ESP_OK;

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_FAIL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_axp_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add AXP2101: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t chip_id = 0;
    ret = read_reg(0x03, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AXP2101 not responding: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "AXP2101 chip ID: 0x%02X", chip_id);

    s_initialized = true;
    ESP_LOGI(TAG, "Battery manager initialized");
    return ESP_OK;
}

esp_err_t battery_manager_get(battery_info_t *out_info)
{
    if (!out_info || !s_initialized) return ESP_ERR_INVALID_STATE;

    memset(out_info, 0, sizeof(*out_info));

    // 1. Power Status (0x00) — VBUS Good на бите 5
    uint8_t status1 = 0;
    bool vbus_good = false;
    if (read_reg(REG_STATUS1, &status1) == ESP_OK) {
        vbus_good = (status1 & 0x20) != 0;   // бит 5 = VBUS Good
        ESP_LOGI(TAG, "STATUS1=0x%02X, VBUS=%d", status1, vbus_good);
    }

    // 2. Charge Status (0x01) — биты [2:0] = статус зарядки
    uint8_t status2 = 0;
    if (read_reg(REG_STATUS2, &status2) == ESP_OK) {
        uint8_t chg_status = status2 & 0x07;   // биты [2:0]
        ESP_LOGI(TAG, "STATUS2=0x%02X, chg_status=%d", status2, chg_status);

        // 0b100 = Charge Done, 0b101 = Not Charging
        bool charge_done = (chg_status == 4);
        bool not_charging = (chg_status == 5);

        out_info->charging    = vbus_good && !not_charging;
        out_info->full        = charge_done;
        out_info->discharging = !vbus_good;
    }

    // 3. Уровень заряда
    uint8_t percent = 0;
    if (read_reg(REG_BAT_PERCENT, &percent) == ESP_OK) {
        if (percent <= 100) {
            out_info->level = percent;
        } else {
            out_info->level = -1;
        }
    }

    // 4. Напряжение
    uint8_t volt_regs[2] = {0};
    if (read_regs(REG_BAT_VOLT_H, volt_regs, 2) == ESP_OK) {
        int raw = ((volt_regs[0] & 0x3F) << 8) | volt_regs[1];
        out_info->voltage_mv = raw;
    }

    ESP_LOGI(TAG, "Battery: %d%%, %dmV, charging=%d, full=%d, discharging=%d",
             out_info->level, out_info->voltage_mv,
             out_info->charging, out_info->full, out_info->discharging);

    return ESP_OK;
}

int battery_manager_get_level(void)
{
    battery_info_t info;
    if (battery_manager_get(&info) == ESP_OK) {
        return info.level;
    }
    return -1;
}

bool battery_manager_is_charging(void)
{
    battery_info_t info;
    if (battery_manager_get(&info) == ESP_OK) {
        return info.charging;
    }
    return false;
}
