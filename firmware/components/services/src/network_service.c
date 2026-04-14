#include "network_service.h"
#include <string.h>
#include "config_models.h"
#include "config_service.h"

error_code_t network_service_init(void) {
    return ERR_OK;
}

error_code_t network_service_connect_from_config(void) {
    device_config_t config;
    wifi_sta_credentials_t credentials;
    error_code_t err = config_service_get(&config);
    if (err != ERR_OK) {
        return err;
    }

    if (!config.wifi.configured) {
        return ERR_WIFI_UNAVAILABLE;
    }

    memset(&credentials, 0, sizeof(credentials));
    strncpy(credentials.ssid, config.wifi.ssid, sizeof(credentials.ssid) - 1U);
    strncpy(credentials.password, config.wifi.password, sizeof(credentials.password) - 1U);

    if (config.network.hostname_set) {
        err = wifi_set_hostname(config.network.hostname);
        if (err != ERR_OK) {
            return err;
        }
    }

    return wifi_connect_sta(&credentials, 10000U);
}

error_code_t network_service_disconnect(void) {
    return wifi_disconnect_sta();
}

error_code_t network_service_scan(wifi_scan_results_t *out_results, uint32_t timeout_ms) {
    return wifi_scan(out_results, timeout_ms);
}

error_code_t network_service_get_status(wifi_status_t *out_status) {
    platform_wifi_status_t platform_status;
    if (!out_status) {
        return ERR_INVALID_ARGS;
    }

    if (wifi_get_status(&platform_status) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(out_status, 0, sizeof(*out_status));
    out_status->mode = platform_status.mode;
    out_status->connected = platform_status.connected;
    out_status->rssi = platform_status.rssi;
    strncpy(out_status->ssid, platform_status.ssid, sizeof(out_status->ssid) - 1U);
    strncpy(out_status->ip_address, platform_status.ip_address, sizeof(out_status->ip_address) - 1U);
    strncpy(out_status->hostname, platform_status.hostname, sizeof(out_status->hostname) - 1U);
    return ERR_OK;
}
