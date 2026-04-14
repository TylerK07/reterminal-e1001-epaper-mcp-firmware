#include "status_service.h"
#include <stdio.h>
#include <string.h>
#include "battery.h"
#include "config_models.h"
#include "config_service.h"
#include "display_epaper.h"
#include "network_service.h"
#include "power.h"
#include "provisioning_service.h"
#include "render_service.h"

error_code_t status_service_init(void) { return ERR_OK; }

error_code_t status_service_get_device_identity(device_identity_t *out_device) {
    device_config_t config;
    if (!out_device) {
        return ERR_INVALID_ARGS;
    }

    memset(out_device, 0, sizeof(*out_device));
    if (config_service_get(&config) == ERR_OK) {
        strncpy(out_device->device_name, config.device_name, sizeof(out_device->device_name) - 1U);
    }
    strncpy(out_device->model, "reTerminal E1001", sizeof(out_device->model) - 1U);
    strncpy(out_device->firmware_version, "0.1.0-skeleton", sizeof(out_device->firmware_version) - 1U);
    out_device->wake_reason = power_get_wake_reason();
    out_device->uptime_ms = 0U;
    return ERR_OK;
}

error_code_t status_service_get_battery_status(battery_status_t *out_status) {
    platform_battery_status_t platform_status;
    if (!out_status) {
        return ERR_INVALID_ARGS;
    }

    if (battery_get_status(&platform_status) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(out_status, 0, sizeof(*out_status));
    out_status->voltage_mv = platform_status.voltage_mv;
    out_status->percent = platform_status.percent;
    out_status->low_battery = platform_status.low_battery;
    out_status->charging = platform_status.charging;
    out_status->power_policy = platform_status.power_policy;
    return ERR_OK;
}

error_code_t status_service_get_wifi_status(wifi_status_t *out_status) {
    return network_service_get_status(out_status);
}

error_code_t status_service_get_display_status(display_status_t *out_status) {
    display_caps_t caps;
    last_render_record_t last_render;

    if (!out_status) {
        return ERR_INVALID_ARGS;
    }

    memset(out_status, 0, sizeof(*out_status));
    if (display_get_caps(&caps) != ERR_OK) {
        return ERR_INTERNAL;
    }

    out_status->initialized = true;
    out_status->busy = display_is_busy();
    out_status->partial_refresh_supported = caps.partial_refresh_supported;
    out_status->resolution.width = caps.width;
    out_status->resolution.height = caps.height;
    if (render_service_get_last_render_record(&last_render) == ERR_OK) {
        out_status->last_refresh_mode = last_render.refresh_mode;
        out_status->last_refresh_elapsed_ms = last_render.elapsed_ms;
    }

    return ERR_OK;
}

error_code_t status_service_get_provisioning_status(provisioning_status_t *out_status) {
    return provisioning_service_get_status(out_status);
}

error_code_t status_service_get_snapshot(device_status_snapshot_t *out_snapshot) {
    if (!out_snapshot) {
        return ERR_INVALID_ARGS;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (status_service_get_device_identity(&out_snapshot->device) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (status_service_get_battery_status(&out_snapshot->battery) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (status_service_get_wifi_status(&out_snapshot->wifi) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (status_service_get_display_status(&out_snapshot->display) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (status_service_get_provisioning_status(&out_snapshot->provisioning) != ERR_OK) {
        return ERR_INTERNAL;
    }

    return ERR_OK;
}

error_code_t status_service_get_snapshot_json(char *buffer, uint32_t buffer_len, uint32_t *out_len) {
    device_status_snapshot_t snapshot;
    int written;

    if (!buffer) {
        return ERR_INVALID_ARGS;
    }

    if (status_service_get_snapshot(&snapshot) != ERR_OK) {
        return ERR_INTERNAL;
    }

    written = snprintf(
        buffer,
        buffer_len,
        "{\"device\":{\"name\":\"%s\",\"model\":\"%s\"},\"battery\":{\"percent\":%u,\"policy\":%u},\"wifi\":{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\"},\"provisioning\":{\"active\":%s}}",
        snapshot.device.device_name,
        snapshot.device.model,
        (unsigned int)snapshot.battery.percent,
        (unsigned int)snapshot.battery.power_policy,
        snapshot.wifi.connected ? "true" : "false",
        snapshot.wifi.ssid,
        snapshot.wifi.ip_address,
        snapshot.provisioning.ap_active ? "true" : "false");
    if (written < 0 || (uint32_t)written >= buffer_len) {
        return ERR_INVALID_ARGS;
    }
    if (out_len) {
        *out_len = (uint32_t)written;
    }
    return ERR_OK;
}
