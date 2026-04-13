#include "services/config_service.h"
#include <string.h>

static device_config_t g_cfg;

error_code_t config_service_init(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.schema_version = 1;
    g_cfg.network.mcp_port = 8080;
    g_cfg.network.mdns_enabled = true;
    g_cfg.display.allow_partial_refresh = true;
    g_cfg.display.show_status_on_connect = true;
    g_cfg.sleep.idle_timeout_sec = 300;
    g_cfg.provisioning.ap_timeout_sec = 900;
    return ERR_OK;
}

error_code_t config_service_get(device_config_t *out_cfg) {
    if (!out_cfg) return ERR_INVALID_ARGS;
    *out_cfg = g_cfg;
    return ERR_OK;
}

error_code_t config_service_get_redacted_json(char *buffer, uint32_t buffer_len, uint32_t *out_len) {
    const char *json = "{\"todo\":\"redacted config json encoder\"}";
    size_t len = strlen(json);
    if (!buffer || buffer_len <= len) return ERR_INVALID_ARGS;
    memcpy(buffer, json, len + 1);
    if (out_len) *out_len = (uint32_t)len;
    return ERR_OK;
}

error_code_t config_service_patch(const config_patch_req_t *patch, bool *out_restart_required, bool *out_reconnect_required) {
    if (!patch) return ERR_INVALID_ARGS;
    if (out_restart_required) *out_restart_required = false;
    if (out_reconnect_required) *out_reconnect_required = false;
    // TODO: apply validated patch fields
    (void)patch;
    return ERR_OK;
}

error_code_t config_service_set_wifi_credentials(const char *ssid, const char *password) {
    if (!ssid || !password) return ERR_INVALID_ARGS;
    // TODO: validate/copy/persist
    (void)ssid; (void)password;
    return ERR_OK;
}

error_code_t config_service_clear_wifi_credentials(void) { return ERR_OK; }
error_code_t config_service_set_auth_token(const char *token) { (void)token; return ERR_OK; }
bool config_service_has_auth_token(void) { return g_cfg.auth_token_set; }
