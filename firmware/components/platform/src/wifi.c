#include "platform/wifi.h"
#include <string.h>

static wifi_status_t g_status;

error_code_t wifi_init(void) {
    memset(&g_status, 0, sizeof(g_status));
    return ERR_OK;
}
error_code_t wifi_set_hostname(const char *hostname) { (void)hostname; return ERR_OK; }
error_code_t wifi_start_ap(const wifi_ap_config_t *cfg, char *out_ip, uint32_t out_ip_len) {
    (void)cfg;
    g_status.mode = WIFI_MODE_AP;
    if (out_ip && out_ip_len > 10) {
        strncpy(out_ip, "192.168.4.1", out_ip_len - 1);
        out_ip[out_ip_len - 1] = '\0';
    }
    return ERR_OK;
}
error_code_t wifi_stop_ap(void) { if (g_status.mode == WIFI_MODE_AP) g_status.mode = WIFI_MODE_OFF; return ERR_OK; }
error_code_t wifi_connect_sta(const wifi_sta_credentials_t *creds, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!creds) return ERR_INVALID_ARGS;
    g_status.mode = WIFI_MODE_STA;
    g_status.connected = true;
    strncpy(g_status.ssid, creds->ssid, sizeof(g_status.ssid) - 1);
    strncpy(g_status.ip_address, "0.0.0.0", sizeof(g_status.ip_address) - 1);
    return ERR_OK;
}
error_code_t wifi_disconnect_sta(void) { g_status.connected = false; return ERR_OK; }
error_code_t wifi_scan(wifi_scan_results_t *out_results, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!out_results) return ERR_INVALID_ARGS;
    memset(out_results, 0, sizeof(*out_results));
    return ERR_OK;
}
error_code_t wifi_get_status(wifi_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    *out_status = g_status;
    return ERR_OK;
}
