#pragma once

#include "lvgl.h"

typedef enum {
    SCREEN_WATCH = 0,
    SCREEN_CHAT,
    SCREEN_MENU,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_SPEEDTEST,
    SCREEN_WIFI,
    SCREEN_WIFI_INFO,
    SCREEN_KEY,
    SCREEN_BRIGHTNESS,
    SCREEN_TIMEZONE,
    SCREEN_RESET,
    SCREEN_TIMER,
    SCREEN_ALARM,
    SCREEN_BLE,
    SCREEN_NET_SPEED,
    SCREEN_CALC,
    SCREEN_MAX
} screen_id_t;

void screen_manager_init(void);
void screen_manager_register(screen_id_t id, lv_obj_t *screen);
void screen_manager_switch(screen_id_t id);
screen_id_t screen_manager_get_current(void);
