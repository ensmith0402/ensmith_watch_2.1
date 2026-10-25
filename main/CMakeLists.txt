idf_component_register(
    SRCS "main.c" "speedtest_ui.c" "rtc_manager.c" "sntp_manager.c" "watch_ui.c" "battery_manager.c" "screen_manager.c" "menu_ui.c" "about_ui.c" "config_manager.c" "settings_ui.c" "settings_key.c" "settings_brightness.c" "settings_timezone.c" "settings_reset.c" "settings_alarm.c" "settings_wifi_info.c" "power_manager.c" "web_api.c" "font_ru_14.c" "font_ru_18.c" "font_ru_20.c" "font_ru_24.c" "font_ru_32.c" "font_ru_48.c" "font_ru_96.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif nvs_flash esp_http_client esp-tls json esp_driver_i2c esp_hw_support esp_http_server esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=cpp)
