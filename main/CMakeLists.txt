idf_component_register(SRCS "myi2c.c" "spi_lcd_touch_example_main.c" "wifi_config_ui.c" "wifi_manager.c" "gxhtc3.c" "wifi_success_ui.c" "uptime_kuma.c" "uptime_kuma_ui.c" "landing_page.c" "main_ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_client json esp_netif esp_wifi nvs_flash)
