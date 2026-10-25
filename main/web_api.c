#include "web_api.h"
#include "config_manager.h"
#include "rtc_manager.h"
#include "battery_manager.h"
#include "esp_log.h"
#include "esp_wifi_config.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "WEB_API";

/* ============================================================
 *  GET /api/status — статус часов
 * ============================================================ */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    // Время
    rtc_time_t now;
    if (rtc_manager_get_time(&now) == ESP_OK) {
        const app_config_t *cfg = config_get();
        int local_hour = (now.hour + cfg->timezone_offset + 24) % 24;

        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", local_hour, now.minute);
        cJSON_AddStringToObject(root, "time", time_str);

        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%02d.%02d.%04d",
                 now.day, now.month, now.year);
        cJSON_AddStringToObject(root, "date", date_str);
    }

    // Батарея
    battery_info_t binfo;
    if (battery_manager_get(&binfo) == ESP_OK) {
        cJSON_AddNumberToObject(root, "battery_level", binfo.level);
        cJSON_AddNumberToObject(root, "battery_voltage", binfo.voltage_mv);
        cJSON_AddBoolToObject(root, "charging", binfo.charging || binfo.full);
    }

    // Wi-Fi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddStringToObject(root, "wifi_ssid", (char *)ap_info.ssid);
        cJSON_AddNumberToObject(root, "wifi_rssi", ap_info.rssi);
        cJSON_AddBoolToObject(root, "wifi_connected", true);

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip.ip));
                cJSON_AddStringToObject(root, "ip", ip_str);
            }
        }
    } else {
        cJSON_AddStringToObject(root, "wifi_ssid", "");
        cJSON_AddBoolToObject(root, "wifi_connected", false);
    }

    // Система
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "uptime_sec", esp_timer_get_time() / 1000000);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);

    return ESP_OK;
}

/* ============================================================
 *  GET /api/settings — текущие настройки
 * ============================================================ */
static esp_err_t settings_get_handler(httpd_req_t *req)
{
    const app_config_t *cfg = config_get();
    cJSON *root = cJSON_CreateObject();

    // SSID берём из Wi-Fi драйвера, а не из нашего config
    wifi_ap_record_t ap_info;
    const char *ssid = "";
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ssid = (const char *)ap_info.ssid;
    }
    cJSON_AddStringToObject(root, "wifi_ssid", ssid);

    cJSON_AddNumberToObject(root, "brightness", cfg->brightness);
    cJSON_AddNumberToObject(root, "timezone_offset", cfg->timezone_offset);

    cJSON_AddBoolToObject(root, "has_wifi_password", true);
    cJSON_AddBoolToObject(root, "has_api_key", strlen(cfg->deepseek_key) > 0);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ============================================================
 *  POST /api/settings — сохранить настройки
 * ============================================================ */
static esp_err_t settings_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Wi-Fi
    cJSON *ssid = cJSON_GetObjectItem(root, "wifi_ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "wifi_password");
    bool wifi_changed = false;
    if (ssid && cJSON_IsString(ssid)) {
        config_set_wifi(ssid->valuestring,
                        (pass && cJSON_IsString(pass)) ? pass->valuestring : "");
        wifi_changed = true;
    }

    // Яркость
    cJSON *bright = cJSON_GetObjectItem(root, "brightness");
    if (bright && cJSON_IsNumber(bright)) {
        config_set_brightness((uint8_t)bright->valueint);
    }

    // Часовой пояс
    cJSON *tz = cJSON_GetObjectItem(root, "timezone_offset");
    if (tz && cJSON_IsNumber(tz)) {
        config_set_timezone((int8_t)tz->valueint);
    }

    // API-ключ
    cJSON *key = cJSON_GetObjectItem(root, "api_key");
    if (key && cJSON_IsString(key)) {
        config_set_deepseek_key(key->valuestring);
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
    ESP_LOGI(TAG, "Настройки сохранены через Web");

    // Если настройки Wi-Fi изменились, перезагружаем часы для их применения
    if (wifi_changed) {
        ESP_LOGW(TAG, "Настройки Wi-Fi изменены. Перезагрузка через 1 сек...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    return ESP_OK;
}

/* ============================================================
 *  POST /api/reboot — перезагрузка
 * ============================================================ */
static esp_err_t reboot_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"rebooting\"}", 22);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ============================================================
 *  POST /api/reset — сброс к заводским
 * ============================================================ */
static esp_err_t reset_post_handler(httpd_req_t *req)
{
    config_reset();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"resetting\"}", 22);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ============================================================
 *  GET /settings — HTML-страница настроек
 * ============================================================ */
static esp_err_t settings_page_handler(httpd_req_t *req)
{
    static const char *html =
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ensmith watch</title>"
        "<style>"
        "body{font-family:sans-serif;background:#111;color:#eee;padding:20px;max-width:600px;margin:auto;font-size:20px}"
        "h1{color:#4af;font-size:36px}"
        "h2{color:#8cf;font-size:28px;margin-top:28px}"
        "input,select{width:100%;padding:14px;margin:6px 0;background:#222;color:#eee;border:1px solid #444;border-radius:8px;font-size:20px}"
        "button{padding:16px 28px;background:#06c;color:#fff;border:none;border-radius:8px;font-size:20px;margin:6px 6px 6px 0;cursor:pointer}"
        "button.danger{background:#c00}"
        ".status{padding:12px;background:#222;border-radius:8px;margin:12px 0;font-size:20px}"
        "</style>"
        "</head><body>"
        "<h1>ensmith watch</h1>"
        "<div class='status' id='status'>Загрузка...</div>"

        "<h2>Wi-Fi</h2>"
        "<div style='display:flex; gap:10px; margin-bottom:10px;'>"
        "<select id='ssid_select' style='flex-grow:1;' onchange=\"document.getElementById('ssid').value = this.value;\">"
        "<option value=''>-- Нажмите 'Сканировать' --</option>"
        "</select>"
        "<button type='button' onclick='scanWifi()'>Сканировать</button>"
        "</div>"
        "<input id='ssid' placeholder='SSID'>"
        "<input id='pass' type='password' placeholder='Пароль'>"

        "<h2>Яркость</h2>"
        "<input id='bright' type='range' min='5' max='100' value='80'>"
        "<span id='bright_val'>80%</span>"

        "<h2>Часовой пояс</h2>"
        "<select id='tz'>"
        "<option value='2'>UTC+2 Калининград</option>"
        "<option value='3' selected>UTC+3 Москва</option>"
        "<option value='4'>UTC+4 Самара</option>"
        "<option value='5'>UTC+5 Екатеринбург</option>"
        "<option value='6'>UTC+6 Омск</option>"
        "<option value='7'>UTC+7 Новосибирск</option>"
        "<option value='8'>UTC+8 Иркутск</option>"
        "<option value='9'>UTC+9 Якутск</option>"
        "<option value='10'>UTC+10 Владивосток</option>"
        "</select>"



        "<h2>Действия</h2>"
        "<button onclick='save()'>Сохранить</button>"
        "<button onclick='reboot()'>Перезагрузить</button>"
        "<button class='danger' onclick='reset()'>Сброс к заводским</button>"

        "<script>"
        "fetch('/api/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('status').innerHTML="
        "'Время: '+d.time+'<br>Батарея: '+d.battery_level+'% '+(d.charging?'(заряжается)':'')+'<br>Wi-Fi: '+(d.wifi_ssid||'не подключён');"
        "});"
        "fetch('/api/settings').then(r=>r.json()).then(d=>{"
        "document.getElementById('ssid').value=d.wifi_ssid;"
        "document.getElementById('bright').value=d.brightness;"
        "document.getElementById('bright_val').innerText=d.brightness+'%';"
        "document.getElementById('tz').value=d.timezone_offset;"
        "});"
        "document.getElementById('bright').oninput=function(){"
        "document.getElementById('bright_val').innerText=this.value+'%';};"
        "function scanWifi() {"
        "  const btn = event.target; btn.innerText = 'Поиск...'; btn.disabled = true;"
        "  fetch('/api/scan').then(r => r.json()).then(data => {"
        "    const select = document.getElementById('ssid_select');"
        "    select.innerHTML = '<option value=\"\">-- Выберите сеть --</option>';"
        "    data.forEach(ap => {"
        "      let opt = document.createElement('option');"
        "      opt.value = ap.ssid; opt.text = ap.ssid + ' (' + ap.rssi + ' dBm)';"
        "      select.appendChild(opt);"
        "    });"
        "    btn.innerText = 'Сканировать'; btn.disabled = false;"
        "  }).catch(e => { alert('Ошибка сканирования'); btn.innerText = 'Сканировать'; btn.disabled = false; });"
        "}"
        "function save(){"
        "fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({"
        "wifi_ssid:document.getElementById('ssid').value,"
        "wifi_password:document.getElementById('pass').value,"
        "brightness:parseInt(document.getElementById('bright').value),"
        "timezone_offset:parseInt(document.getElementById('tz').value),"
        "})}).then(r=>r.json()).then(d=>alert('Сохранено!'));}"
        "function reboot(){"
        "if(confirm('Перезагрузить часы?'))"
        "fetch('/api/reboot',{method:'POST'});}"
        "function reset(){"
        "if(confirm('Сбросить к заводским? Все настройки будут удалены!'))"
        "fetch('/api/reset',{method:'POST'});}"
        "</script>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

/* ============================================================
 *  Регистрация endpoints
 * ============================================================ */

/* ============================================================
 *  GET /api/scan — сканирование Wi-Fi сетей
 * ============================================================ */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)ap_list[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_list[i].rssi);
        cJSON_AddItemToArray(root, ap);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    free(json_str);
    cJSON_Delete(root);
    free(ap_list);
    return ESP_OK;
}

esp_err_t web_api_init(void)
{
    httpd_handle_t server = wifi_cfg_get_httpd();
    if (!server) {
        ESP_LOGE(TAG, "HTTP server not available");
        return ESP_FAIL;
    }

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
    };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t settings_get_uri = {
        .uri = "/api/settings",
        .method = HTTP_GET,
        .handler = settings_get_handler,
    };
    httpd_register_uri_handler(server, &settings_get_uri);

    httpd_uri_t settings_post_uri = {
        .uri = "/api/settings",
        .method = HTTP_POST,
        .handler = settings_post_handler,
    };
    httpd_register_uri_handler(server, &settings_post_uri);

    httpd_uri_t reboot_uri = {
        .uri = "/api/reboot",
        .method = HTTP_POST,
        .handler = reboot_post_handler,
    };
    httpd_register_uri_handler(server, &reboot_uri);

    httpd_uri_t reset_uri = {
        .uri = "/api/reset",
        .method = HTTP_POST,
        .handler = reset_post_handler,
    };
    httpd_register_uri_handler(server, &reset_uri);

    httpd_uri_t page_uri = {
        .uri = "/settings",
        .method = HTTP_GET,
        .handler = settings_page_handler,
    };
    httpd_register_uri_handler(server, &page_uri);

    httpd_uri_t scan_uri = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = scan_get_handler,
    };
    httpd_register_uri_handler(server, &scan_uri);

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = settings_page_handler,
    };
    httpd_register_uri_handler(server, &root_uri);

    ESP_LOGI(TAG, "Web API зарегистрирован: /settings, /api/status, /api/settings, /api/reboot, /api/reset");
    return ESP_OK;
}
