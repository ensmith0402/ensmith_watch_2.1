#include "deepseek_client.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "DEEPSEEK";
static char *s_api_key = NULL;

#define DEEPSEEK_URL "https://api.deepseek.com/v1/chat/completions"
#define MAX_RESPONSE_SIZE 4096

typedef struct {
    char *buffer;
    int len;
    int max;
} response_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    response_buf_t *rb = (response_buf_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (rb && evt->data_len > 0) {
            int copy = evt->data_len;
            if (rb->len + copy >= rb->max - 1) {
                copy = rb->max - 1 - rb->len;
            }
            if (copy > 0) {
                memcpy(rb->buffer + rb->len, evt->data, copy);
                rb->len += copy;
                rb->buffer[rb->len] = '\0';
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

void deepseek_set_api_key(const char *api_key)
{
    if (s_api_key) free(s_api_key);
    s_api_key = strdup(api_key);
}

esp_err_t deepseek_send(const char *prompt, char **out_response)
{
    if (!s_api_key || !prompt || !out_response) {
        ESP_LOGE(TAG, "Не задан API-ключ или prompt");
        return ESP_ERR_INVALID_ARG;
    }

    // Формируем JSON-запрос
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "deepseek-flash");
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", prompt);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddBoolToObject(root, "stream", 0);
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    // Буфер для ответа
    char *resp_buf = malloc(MAX_RESPONSE_SIZE);
    if (!resp_buf) {
        free(post_data);
        return ESP_ERR_NO_MEM;
    }
    resp_buf[0] = '\0';
    response_buf_t rb = { .buffer = resp_buf, .len = 0, .max = MAX_RESPONSE_SIZE };

    // HTTP-клиент
    esp_http_client_config_t config = {
        .url = DEEPSEEK_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &rb,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", s_api_key);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Отправка запроса...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP статус: %d", status);
        if (status == 200) {
            cJSON *json = cJSON_Parse(resp_buf);
            if (json) {
                cJSON *choices = cJSON_GetObjectItem(json, "choices");
                if (choices && cJSON_IsArray(choices)) {
                    cJSON *first = cJSON_GetArrayItem(choices, 0);
                    cJSON *message = cJSON_GetObjectItem(first, "message");
                    cJSON *content = cJSON_GetObjectItem(message, "content");
                    if (content && cJSON_IsString(content)) {
                        *out_response = strdup(content->valuestring);
                        cJSON_Delete(json);
                        free(post_data);
                        free(resp_buf);
                        esp_http_client_cleanup(client);
                        return ESP_OK;
                    }
                }
                cJSON_Delete(json);
            }
            ESP_LOGE(TAG, "Не удалось распарсить ответ");
        } else {
            ESP_LOGE(TAG, "Ошибка API: %s", resp_buf);
        }
    } else {
        ESP_LOGE(TAG, "HTTP ошибка: %s", esp_err_to_name(err));
    }

    free(post_data);
    free(resp_buf);
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}
