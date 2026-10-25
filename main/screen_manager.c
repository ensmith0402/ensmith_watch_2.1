#include "screen_manager.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_MGR";

static lv_obj_t *s_screens[SCREEN_MAX] = {0};
static screen_id_t s_current = SCREEN_WATCH;

void screen_manager_init(void)
{
    for (int i = 0; i < SCREEN_MAX; i++) {
        s_screens[i] = NULL;
    }
    s_current = SCREEN_WATCH;
    ESP_LOGI(TAG, "Screen manager initialized");
}

void screen_manager_register(screen_id_t id, lv_obj_t *screen)
{
    if (id >= SCREEN_MAX) return;
    s_screens[id] = screen;
    ESP_LOGI(TAG, "Screen %d registered", id);
}

void screen_manager_switch(screen_id_t id)
{
    if (id >= SCREEN_MAX) return;
    if (!s_screens[id]) {
        ESP_LOGW(TAG, "Screen %d not registered", id);
        return;
    }

    s_current = id;
    lv_scr_load(s_screens[id]);
    ESP_LOGI(TAG, "Switched to screen %d", id);
}

screen_id_t screen_manager_get_current(void)
{
    return s_current;
}
