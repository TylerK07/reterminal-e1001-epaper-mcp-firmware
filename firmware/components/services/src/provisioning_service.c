#include "provisioning_service.h"
#include <string.h>
#include "config_models.h"
#include "config_service.h"
#include "provisioning_http.h"
#include "wifi.h"

static bool g_active = false;
static provisioning_status_t g_status;

static error_code_t provisioning_service_submit_from_http(const provisioning_http_submission_t *submission) {
    provisioning_submission_t request;

    if (!submission) {
        return ERR_INVALID_ARGS;
    }

    memset(&request, 0, sizeof(request));
    strncpy(request.ssid, submission->ssid, sizeof(request.ssid) - 1U);
    strncpy(request.password, submission->password, sizeof(request.password) - 1U);
    strncpy(request.device_name, submission->device_name, sizeof(request.device_name) - 1U);
    strncpy(request.auth_token, submission->auth_token, sizeof(request.auth_token) - 1U);
    return provisioning_service_submit(&request);
}

error_code_t provisioning_service_init(void) {
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = config_service_is_provisioned() ? PROVISIONING_STATE_CONNECTED : PROVISIONING_STATE_UNPROVISIONED;
    if (provisioning_http_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

error_code_t provisioning_service_start(void) {
    device_config_t config;
    wifi_ap_config_t ap_config;
    provisioning_http_handlers_t handlers;
    error_code_t err;

    if (config_service_get(&config) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(&ap_config, 0, sizeof(ap_config));
    strncpy(ap_config.ssid, "reterminal-e1001-setup", sizeof(ap_config.ssid) - 1U);
    strncpy(ap_config.password, config.provisioning.ap_password, sizeof(ap_config.password) - 1U);
    ap_config.wpa2_enabled = config.provisioning.ap_wpa2_enabled;
    ap_config.channel = 1U;

    err = wifi_start_ap(&ap_config, g_status.ap_ip_address, sizeof(g_status.ap_ip_address));
    if (err != ERR_OK) {
        return err;
    }

    memset(&handlers, 0, sizeof(handlers));
    handlers.submit = provisioning_service_submit_from_http;

    err = provisioning_http_start(&handlers);
    if (err != ERR_OK) {
        (void)wifi_stop_ap();
        return err;
    }

    g_active = true;
    g_status.state = PROVISIONING_STATE_ACTIVE;
    g_status.ap_active = true;
    g_status.timeout_remaining_sec = config.provisioning.ap_timeout_sec;
    strncpy(g_status.ap_ssid, ap_config.ssid, sizeof(g_status.ap_ssid) - 1U);
    return ERR_OK;
}

error_code_t provisioning_service_stop(void) {
    (void)provisioning_http_stop();
    g_active = false;
    g_status.ap_active = false;
    if (g_status.state == PROVISIONING_STATE_ACTIVE) {
        g_status.state = PROVISIONING_STATE_UNPROVISIONED;
    }
    return wifi_stop_ap();
}

bool provisioning_service_is_active(void) { return g_active; }

error_code_t provisioning_service_get_status(provisioning_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    *out_status = g_status;
    return ERR_OK;
}

error_code_t provisioning_service_submit(const provisioning_submission_t *submission) {
    config_patch_req_t patch;
    if (!submission) return ERR_INVALID_ARGS;

    if (submission->ssid[0] == '\0' || submission->device_name[0] == '\0') {
        return ERR_INVALID_ARGS;
    }

    if (config_service_set_wifi_credentials(submission->ssid, submission->password) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(&patch, 0, sizeof(patch));
    patch.set_device_name = true;
    strncpy(patch.device_name, submission->device_name, sizeof(patch.device_name) - 1U);
    (void)config_service_patch(&patch, NULL, NULL);

    if (submission->auth_token[0] != '\0') {
        (void)config_service_set_auth_token(submission->auth_token);
    }

    g_status.state = PROVISIONING_STATE_CONNECTED;
    g_active = false;
    g_status.ap_active = false;
    return ERR_OK;
}

error_code_t provisioning_service_request_reset(void) {
    (void)provisioning_http_stop();
    g_active = false;
    g_status.state = PROVISIONING_STATE_UNPROVISIONED;
    g_status.ap_active = false;
    g_status.ap_ssid[0] = '\0';
    g_status.ap_ip_address[0] = '\0';
    g_status.timeout_remaining_sec = 0U;
    return config_service_clear_wifi_credentials();
}

error_code_t provisioning_service_reset(void) {
    (void)provisioning_service_request_reset();
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = PROVISIONING_STATE_UNPROVISIONED;
    return wifi_stop_ap();
}
