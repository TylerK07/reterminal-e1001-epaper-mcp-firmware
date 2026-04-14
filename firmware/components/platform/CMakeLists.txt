idf_component_register(
    SRCS
        "src/battery.c"
        "src/board.c"
        "src/display_epaper.c"
        "src/power.c"
        "src/provisioning_http.c"
        "src/storage_sd.c"
        "src/wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_event esp_http_server esp_netif esp_wifi nvs_flash
)
