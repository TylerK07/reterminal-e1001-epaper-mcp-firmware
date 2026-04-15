idf_component_register(
    SRCS
        "src/battery.c"
        "src/board.c"
        "src/buttons.c"
        "src/display_epaper.c"
        "src/environment_sensor.c"
        "src/network_mdns.c"
        "src/power.c"
        "src/provisioning_http.c"
        "src/storage_sd.c"
        "src/storage_nvs.c"
        "src/wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_event esp_http_server esp_netif esp_wifi nvs_flash
)
