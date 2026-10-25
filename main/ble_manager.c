#include "ble_manager.h"
#include "esp_log.h"
#include <string.h>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLE";

#define DEVICE_NAME             "ensmith watch"
#define PROFILE_NUM             1
#define PROFILE_APP_ID          0

static const uint16_t SERVICE_UUID  = 0x00FF;
static const uint16_t CHAR_UUID_RX  = 0xFF01;   // телефон → часы (write)
static const uint16_t CHAR_UUID_TX  = 0xFF02;   // часы → телефон (notify)

static bool s_connected = false;
static uint16_t s_conn_id = 0;
static uint16_t s_char_tx_handle = 0;
static esp_gatt_if_t s_gatts_if = 0;

static uint16_t service_handle;
static uint16_t char_rx_handle;
static uint16_t char_tx_handle;

/* ---------- GAP ---------- */
static uint8_t adv_service_uuid128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(adv_service_uuid128),
    .p_service_uuid      = adv_service_uuid128,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ---------- GATT ---------- */
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t char_declare_uuid     = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t  char_prop_rw          = ESP_GATT_CHAR_PROP_BIT_WRITE |
                                              ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint8_t char_value[20] = {0};

static const esp_gatts_attr_db_t gatt_db[] = {
    // Service Declaration
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(SERVICE_UUID), (uint8_t *)&SERVICE_UUID}
    },
    // Char RX Declaration (write)
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_declare_uuid, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_rw}
    },
    // Char RX Value
    [2] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&CHAR_UUID_RX, ESP_GATT_PERM_WRITE,
         sizeof(char_value), sizeof(char_value), (uint8_t *)char_value}
    },
    // Char TX Declaration (notify)
    [3] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_declare_uuid, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_rw}
    },
    // Char TX Value
    [4] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&CHAR_UUID_TX, ESP_GATT_PERM_READ,
         sizeof(char_value), sizeof(char_value), (uint8_t *)char_value}
    },
};

/* ---------- GAP callback ---------- */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Реклама запущена");
        } else {
            ESP_LOGE(TAG, "Ошибка рекламы: %d", param->adv_start_cmpl.status);
        }
        break;
    default:
        break;
    }
}

/* ---------- GATT callback ---------- */
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                 esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        s_gatts_if = gatts_if;
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 5, PROFILE_APP_ID);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Ошибка создания таблицы: 0x%x", param->add_attr_tab.status);
            break;
        }
        service_handle = param->add_attr_tab.handles[0];
        char_rx_handle  = param->add_attr_tab.handles[2];
        char_tx_handle  = param->add_attr_tab.handles[4];
        s_char_tx_handle = char_tx_handle;
        esp_ble_gatts_start_service(service_handle);
        ESP_LOGI(TAG, "GATT сервис запущен");
        break;

    case ESP_GATTS_CONNECT_EVT:
        s_connected = true;
        s_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "Клиент подключён");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        s_connected = false;
        ESP_LOGI(TAG, "Клиент отключён, перезапуск рекламы");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == char_rx_handle) {
            ESP_LOGI(TAG, "Принято от телефона: %.*s",
                     param->write.len, param->write.value);
        }
        break;

    default:
        break;
    }
}

/* ---------- Публичные функции ---------- */
esp_err_t ble_manager_init(void)
{
    esp_err_t ret;

    // Освобождаем память классического BT (нам нужен только BLE)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));

    ESP_LOGI(TAG, "BLE инициализирован, имя: %s", DEVICE_NAME);
    return ESP_OK;
}

bool ble_manager_is_connected(void)
{
    return s_connected;
}

esp_err_t ble_manager_notify(const char *text)
{
    if (!s_connected || !text) return ESP_FAIL;

    return esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id,
                                        s_char_tx_handle,
                                        strlen(text), (uint8_t *)text, false);
}
