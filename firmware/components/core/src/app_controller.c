#include "app_controller.h"
#include <stdbool.h>
#include "asset_service.h"
#include "battery.h"
#include "buttons.h"
#include "config_models.h"
#include "config_service.h"
#include "display_epaper.h"
#include "environment_sensor.h"
#include "mcp_dispatch.h"
#include "mcp_server.h"
#include "mcp_registry.h"
#include "network_service.h"
#include "power.h"
#include "provisioning_service.h"
#include "render_service.h"
#include "status_service.h"
#include "storage_sd.h"
#include "wifi.h"

static app_state_t g_state = STATE_BOOTING;
static bool g_mcp_enabled = false;

static error_code_t app_controller_start_provisioning(void);
static error_code_t app_controller_enable_mcp_if_allowed(void);

static error_code_t app_controller_enter_provisioning_runtime(void) {
    (void)network_service_disconnect();
    return app_controller_start_provisioning();
}

static error_code_t app_controller_connect_runtime(void) {
    error_code_t err;

    g_state = STATE_CONNECTING;
    err = network_service_connect_from_config();
    if (err != ERR_OK) {
        g_state = STATE_ERROR_RECOVERY;
        (void)provisioning_service_reset();
        return app_controller_start_provisioning();
    }

    g_state = STATE_CONNECTED_IDLE;

    err = network_service_start_discovery_from_config();
    if (err != ERR_OK) {
        return err;
    }

    err = app_controller_enable_mcp_if_allowed();
    if (err != ERR_OK) {
        g_mcp_enabled = false;
        return err;
    }

    return ERR_OK;
}

static error_code_t app_controller_start_provisioning(void) {
    (void)network_service_stop_discovery();
    g_mcp_enabled = false;
    (void)mcp_server_stop();

    error_code_t err = provisioning_service_start();
    if (err != ERR_OK) {
        g_state = STATE_ERROR_RECOVERY;
        return err;
    }

    g_state = STATE_PROVISIONING;
    return ERR_OK;
}

static error_code_t app_controller_enable_mcp_if_allowed(void) {
    device_config_t config;
    power_policy_t policy;
    error_code_t err = config_service_get(&config);
    if (err != ERR_OK) {
        return err;
    }

    err = power_get_policy(&policy);
    if (err != ERR_OK) {
        return err;
    }

    err = mcp_server_start(config.network.mcp_port);
    if (err != ERR_OK) {
        g_mcp_enabled = false;
        return err;
    }

    if (!config_service_has_auth_token() || !power_operation_allowed(policy, "mcp_server")) {
        g_mcp_enabled = false;
        return ERR_OK;
    }

    g_mcp_enabled = true;
    return ERR_OK;
}

error_code_t app_controller_init(void) {
    g_state = STATE_BOOTING;
    g_mcp_enabled = false;

    if (power_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (storage_sd_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    (void)storage_sd_mount();
    if (config_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (battery_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (buttons_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (wifi_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (display_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (environment_sensor_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (asset_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (network_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (render_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (status_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (provisioning_service_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (mcp_registry_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (mcp_dispatch_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (mcp_server_init() != ERR_OK) {
        return ERR_INTERNAL;
    }

    return ERR_OK;
}

error_code_t app_controller_run(void) {
    bool button_override_requested = false;

    if (buttons_is_any_pressed(&button_override_requested) == ERR_OK && button_override_requested) {
        (void)config_service_clear_wifi_credentials();
        g_state = STATE_UNPROVISIONED;
        return app_controller_start_provisioning();
    }

    if (!config_service_is_provisioned()) {
        g_state = STATE_UNPROVISIONED;
        return app_controller_start_provisioning();
    }

    return app_controller_connect_runtime();
}

error_code_t app_controller_tick(void) {
    provisioning_status_t provisioning_status;
    error_code_t err;

    if (g_state != STATE_PROVISIONING) {
        if (!config_service_is_provisioned()) {
            g_state = STATE_UNPROVISIONED;
            return app_controller_enter_provisioning_runtime();
        }
        return ERR_OK;
    }

    if (provisioning_service_get_status(&provisioning_status) != ERR_OK) {
        return ERR_INTERNAL;
    }

    if (provisioning_status.state == PROVISIONING_STATE_CONNECTED && config_service_is_provisioned()) {
        err = provisioning_service_stop();
        if (err != ERR_OK) {
            return err;
        }
        return app_controller_connect_runtime();
    }

    return ERR_OK;
}

app_state_t app_controller_get_state(void) {
    return g_state;
}

bool app_controller_is_mcp_enabled(void) {
    return g_mcp_enabled;
}
