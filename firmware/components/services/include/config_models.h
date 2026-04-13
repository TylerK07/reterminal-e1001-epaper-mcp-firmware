#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char ssid[64];
    char password[128];
    bool configured;
} config_wifi_t;

typedef struct {
    bool mdns_enabled;
    char hostname[64];
    uint16_t mcp_port;
} config_network_t;

typedef struct {
    uint32_t idle_timeout_sec;
    uint32_t active_timeout_sec;
    bool sleep_on_low_battery;
    uint8_t low_battery_threshold_percent;
    uint8_t critical_battery_threshold_percent;
} config_sleep_t;

typedef struct {
    uint32_t ap_timeout_sec;
    bool ap_wpa2_enabled;
    char ap_password[64];
} config_provisioning_t;

typedef struct {
    bool show_status_on_connect;
    bool allow_partial_refresh;
    uint16_t default_font_size;
    char default_font_name[32];
} config_display_t;

typedef struct {
    uint16_t schema_version;
    char device_name[64];
    char auth_token[128];
    bool auth_token_set;
    config_wifi_t wifi;
    config_network_t network;
    config_sleep_t sleep;
    config_provisioning_t provisioning;
    config_display_t display;
} device_config_t;

typedef struct {
    bool set_device_name;
    char device_name[64];
    bool set_mdns_enabled;
    bool mdns_enabled;
    bool set_hostname;
    char hostname[64];
    bool set_ap_timeout_sec;
    uint32_t ap_timeout_sec;
    bool set_show_status_on_connect;
    bool show_status_on_connect;
} config_patch_req_t;
