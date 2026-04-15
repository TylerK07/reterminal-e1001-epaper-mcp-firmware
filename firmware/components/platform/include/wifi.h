#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "enums.h"
#include "errors.h"

#define WIFI_MAX_SCAN_RESULTS 50

typedef struct {
    char ssid[64];
    int16_t rssi;
    char auth_mode[16];
} wifi_scan_entry_t;

typedef struct {
    uint16_t count;
    wifi_scan_entry_t entries[WIFI_MAX_SCAN_RESULTS];
} wifi_scan_results_t;

typedef struct {
    char ssid[64];
    char password[128];
} wifi_sta_credentials_t;

typedef struct {
    char ssid[64];
    char password[64];
    bool wpa2_enabled;
    uint8_t channel;
} wifi_ap_config_t;

typedef struct {
    wifi_mode_t mode;
    bool connected;
    char ssid[64];
    char ip_address[40];
    char hostname[64];
    int16_t rssi;
} platform_wifi_status_t;

error_code_t wifi_init(void);
error_code_t wifi_set_hostname(const char *hostname);
error_code_t wifi_start_ap(const wifi_ap_config_t *cfg, char *out_ip, uint32_t out_ip_len);
error_code_t wifi_stop_ap(void);
error_code_t wifi_connect_sta(const wifi_sta_credentials_t *creds, uint32_t timeout_ms);
error_code_t wifi_disconnect_sta(void);
error_code_t wifi_scan(wifi_scan_results_t *out_results, uint32_t timeout_ms);
error_code_t wifi_get_status(platform_wifi_status_t *out_status);
