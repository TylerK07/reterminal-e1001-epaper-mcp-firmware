#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "enums.h"
#include "types.h"

typedef struct {
    wifi_mode_t mode;
    bool connected;
    char ssid[64];
    char ip_address[40];
    char hostname[64];
    int16_t rssi;
} wifi_status_t;

typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
    bool low_battery;
    bool charging;
    power_policy_t power_policy;
} battery_status_t;

typedef struct {
    bool initialized;
    bool busy;
    bool partial_refresh_supported;
    size_u16_t resolution;
    display_refresh_mode_t last_refresh_mode;
    uint32_t last_refresh_elapsed_ms;
} display_status_t;

typedef enum {
    PROVISIONING_STATE_UNPROVISIONED = 0,
    PROVISIONING_STATE_ACTIVE,
    PROVISIONING_STATE_CONNECTED
} provisioning_state_t;

typedef struct {
    provisioning_state_t state;
    bool ap_active;
    char ap_ssid[64];
    char ap_ip_address[40];
    uint32_t timeout_remaining_sec;
} provisioning_status_t;

typedef struct {
    char model[32];
    char firmware_version[32];
    char device_name[64];
    wake_reason_t wake_reason;
    uint32_t uptime_ms;
} device_identity_t;

typedef struct {
    device_identity_t device;
    battery_status_t battery;
    wifi_status_t wifi;
    display_status_t display;
    provisioning_status_t provisioning;
} device_status_snapshot_t;
