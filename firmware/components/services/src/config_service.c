#include "config_service.h"
#include <stdio.h>
#include <string.h>
#include "storage_nvs.h"

static device_config_t g_cfg;

#define CONFIG_SERVICE_NVS_NAMESPACE "cfg"
#define CONFIG_SERVICE_NVS_KEY "device"

static void config_service_apply_defaults(device_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->schema_version = 1U;
    cfg->network.mcp_port = 8080U;
    cfg->network.mdns_enabled = true;
    cfg->display.allow_partial_refresh = true;
    cfg->display.show_status_on_connect = true;
    cfg->display.default_font_size = 24U;
    strncpy(cfg->display.default_font_name, "default", sizeof(cfg->display.default_font_name) - 1U);
    cfg->sleep.idle_timeout_sec = 300U;
    cfg->sleep.active_timeout_sec = 900U;
    cfg->sleep.sleep_on_low_battery = true;
    cfg->sleep.low_battery_threshold_percent = 20U;
    cfg->sleep.critical_battery_threshold_percent = 10U;
    cfg->provisioning.ap_timeout_sec = 900U;
    cfg->provisioning.ap_wpa2_enabled = false;
    strncpy(cfg->device_name, "reterminal-e1001", sizeof(cfg->device_name) - 1U);
}

static error_code_t config_service_persist(void) {
    return storage_nvs_write_blob(CONFIG_SERVICE_NVS_NAMESPACE, CONFIG_SERVICE_NVS_KEY, &g_cfg, sizeof(g_cfg));
}

static void config_service_sanitize_loaded(device_config_t *cfg) {
    if (!cfg) {
        return;
    }

    cfg->device_name[sizeof(cfg->device_name) - 1U] = '\0';
    cfg->auth_token[sizeof(cfg->auth_token) - 1U] = '\0';
    cfg->wifi.ssid[sizeof(cfg->wifi.ssid) - 1U] = '\0';
    cfg->wifi.password[sizeof(cfg->wifi.password) - 1U] = '\0';
    cfg->network.hostname[sizeof(cfg->network.hostname) - 1U] = '\0';
    cfg->provisioning.ap_password[sizeof(cfg->provisioning.ap_password) - 1U] = '\0';
    cfg->display.default_font_name[sizeof(cfg->display.default_font_name) - 1U] = '\0';

    cfg->auth_token_set = (cfg->auth_token[0] != '\0');
    cfg->wifi.configured = (cfg->wifi.ssid[0] != '\0');
    cfg->network.hostname_set = (cfg->network.hostname[0] != '\0');
}

error_code_t config_service_init(void) {
    uint32_t loaded_len = 0U;
    error_code_t err;

    config_service_apply_defaults(&g_cfg);
    if (storage_nvs_init() != ERR_OK) {
        return ERR_INTERNAL;
    }

    err = storage_nvs_read_blob(CONFIG_SERVICE_NVS_NAMESPACE, CONFIG_SERVICE_NVS_KEY, &g_cfg, sizeof(g_cfg), &loaded_len);
    if (err == ERR_OK) {
        if (loaded_len == sizeof(g_cfg) && g_cfg.schema_version == 1U) {
            config_service_sanitize_loaded(&g_cfg);
            return ERR_OK;
        }
        config_service_apply_defaults(&g_cfg);
        return config_service_persist();
    }
    if (err != ERR_NOT_FOUND) {
        return err;
    }

    return config_service_persist();
}

error_code_t config_service_reset_defaults(void) {
    char preserved_ssid[sizeof(g_cfg.wifi.ssid)];
    char preserved_password[sizeof(g_cfg.wifi.password)];
    bool wifi_configured = g_cfg.wifi.configured;

    strncpy(preserved_ssid, g_cfg.wifi.ssid, sizeof(preserved_ssid) - 1U);
    preserved_ssid[sizeof(preserved_ssid) - 1U] = '\0';
    strncpy(preserved_password, g_cfg.wifi.password, sizeof(preserved_password) - 1U);
    preserved_password[sizeof(preserved_password) - 1U] = '\0';

    config_service_apply_defaults(&g_cfg);
    if (wifi_configured) {
        (void)config_service_set_wifi_credentials(preserved_ssid, preserved_password);
    }
    return config_service_persist();
}

error_code_t config_service_get(device_config_t *out_cfg) {
    if (!out_cfg) return ERR_INVALID_ARGS;
    *out_cfg = g_cfg;
    return ERR_OK;
}

error_code_t config_service_get_redacted_json(char *buffer, uint32_t buffer_len, uint32_t *out_len) {
    int written;
    if (!buffer) {
        return ERR_INVALID_ARGS;
    }

    written = snprintf(
        buffer,
        buffer_len,
        "{\"schema_version\":%u,\"device_name\":\"%s\",\"auth_token\":null,\"wifi\":{\"configured\":%s,\"ssid\":\"%s\",\"password\":null},\"network\":{\"mdns_enabled\":%s,\"hostname\":\"%s\",\"mcp_port\":%u}}",
        (unsigned int)g_cfg.schema_version,
        g_cfg.device_name,
        g_cfg.wifi.configured ? "true" : "false",
        g_cfg.wifi.ssid,
        g_cfg.network.mdns_enabled ? "true" : "false",
        g_cfg.network.hostname,
        (unsigned int)g_cfg.network.mcp_port);
    if (written < 0 || (uint32_t)written >= buffer_len) {
        return ERR_INVALID_ARGS;
    }
    if (out_len) {
        *out_len = (uint32_t)written;
    }
    return ERR_OK;
}

error_code_t config_service_patch(const config_patch_req_t *patch, bool *out_restart_required, bool *out_reconnect_required) {
    if (!patch) {
        return ERR_INVALID_ARGS;
    }

    if (out_restart_required) {
        *out_restart_required = false;
    }
    if (out_reconnect_required) {
        *out_reconnect_required = false;
    }

    if (patch->set_device_name) {
        strncpy(g_cfg.device_name, patch->device_name, sizeof(g_cfg.device_name) - 1U);
    }
    if (patch->set_mdns_enabled) {
        g_cfg.network.mdns_enabled = patch->mdns_enabled;
    }
    if (patch->set_hostname) {
        strncpy(g_cfg.network.hostname, patch->hostname, sizeof(g_cfg.network.hostname) - 1U);
        g_cfg.network.hostname_set = (patch->hostname[0] != '\0');
        if (out_reconnect_required) {
            *out_reconnect_required = true;
        }
    }
    if (patch->set_ap_timeout_sec) {
        g_cfg.provisioning.ap_timeout_sec = patch->ap_timeout_sec;
    }
    if (patch->set_show_status_on_connect) {
        g_cfg.display.show_status_on_connect = patch->show_status_on_connect;
    }

    return config_service_persist();
}

error_code_t config_service_set_wifi_credentials(const char *ssid, const char *password) {
    if (!ssid || !password) {
        return ERR_INVALID_ARGS;
    }

    strncpy(g_cfg.wifi.ssid, ssid, sizeof(g_cfg.wifi.ssid) - 1U);
    g_cfg.wifi.ssid[sizeof(g_cfg.wifi.ssid) - 1U] = '\0';
    strncpy(g_cfg.wifi.password, password, sizeof(g_cfg.wifi.password) - 1U);
    g_cfg.wifi.password[sizeof(g_cfg.wifi.password) - 1U] = '\0';
    g_cfg.wifi.configured = (g_cfg.wifi.ssid[0] != '\0');
    return config_service_persist();
}

error_code_t config_service_clear_wifi_credentials(void) {
    memset(&g_cfg.wifi, 0, sizeof(g_cfg.wifi));
    return config_service_persist();
}

error_code_t config_service_set_auth_token(const char *token) {
    if (!token) {
        return ERR_INVALID_ARGS;
    }

    strncpy(g_cfg.auth_token, token, sizeof(g_cfg.auth_token) - 1U);
    g_cfg.auth_token[sizeof(g_cfg.auth_token) - 1U] = '\0';
    g_cfg.auth_token_set = (g_cfg.auth_token[0] != '\0');
    return config_service_persist();
}

error_code_t config_service_get_auth_token(char *buffer, uint32_t buffer_len) {
    size_t len;
    if (!buffer || buffer_len == 0U) {
        return ERR_INVALID_ARGS;
    }

    len = strnlen(g_cfg.auth_token, sizeof(g_cfg.auth_token));
    if (len + 1U > buffer_len) {
        return ERR_INVALID_ARGS;
    }

    memcpy(buffer, g_cfg.auth_token, len + 1U);
    return ERR_OK;
}

bool config_service_has_auth_token(void) {
    return g_cfg.auth_token_set;
}

bool config_service_is_provisioned(void) {
    return g_cfg.wifi.configured;
}

bool config_service_token_matches(const char *token) {
    if (!token || !g_cfg.auth_token_set) {
        return false;
    }

    return strncmp(token, g_cfg.auth_token, sizeof(g_cfg.auth_token)) == 0;
}
