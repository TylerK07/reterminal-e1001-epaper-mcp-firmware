#include "app_controller.h"
#include <inttypes.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include "render_service.h"
#include "status_service.h"

#ifdef ESP_PLATFORM
static render_result_t g_status_screen_result;
static render_status_screen_t g_status_screen;
static render_status_screen_t g_last_rendered_status_screen;
static bool g_status_screen_rendered = false;
#endif

#ifdef ESP_PLATFORM
static const char *TAG = "app_main";

static void app_main_prepare_screen_compare_model(render_status_screen_t *screen) {
    if (!screen) {
        return;
    }

    memset(screen->snapshot.device.model, 0, sizeof(screen->snapshot.device.model));
    memset(screen->snapshot.device.firmware_version, 0, sizeof(screen->snapshot.device.firmware_version));
    screen->snapshot.device.wake_reason = WAKE_REASON_UNKNOWN;
    screen->snapshot.device.uptime_ms = 0U;

    screen->snapshot.battery.voltage_mv = 0U;
    screen->snapshot.battery.low_battery = false;

    memset(screen->snapshot.wifi.hostname, 0, sizeof(screen->snapshot.wifi.hostname));
    screen->snapshot.wifi.rssi = 0;
    if (!screen->snapshot.wifi.connected) {
        memset(screen->snapshot.wifi.ssid, 0, sizeof(screen->snapshot.wifi.ssid));
    }

    memset(&screen->snapshot.display, 0, sizeof(screen->snapshot.display));
    memset(&screen->snapshot.environment, 0, sizeof(screen->snapshot.environment));

    if (!screen->snapshot.provisioning.ap_active) {
        memset(screen->snapshot.provisioning.ap_ssid, 0, sizeof(screen->snapshot.provisioning.ap_ssid));
        memset(screen->snapshot.provisioning.ap_ip_address, 0, sizeof(screen->snapshot.provisioning.ap_ip_address));
    }
    screen->snapshot.provisioning.timeout_remaining_sec = 0U;
}

static const char *app_state_to_string(app_state_t state) {
    switch (state) {
        case STATE_BOOTING: return "booting";
        case STATE_UNPROVISIONED: return "unprovisioned";
        case STATE_PROVISIONING: return "provisioning";
        case STATE_CONNECTING: return "connecting";
        case STATE_CONNECTED_IDLE: return "connected_idle";
        case STATE_CONNECTED_ACTIVE: return "connected_active";
        case STATE_SLEEP_PREP: return "sleep_prep";
        case STATE_SLEEPING: return "sleeping";
        case STATE_ERROR_RECOVERY: return "error_recovery";
        default: return "unknown";
    }
}

static const char *wifi_mode_to_string(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_OFF: return "off";
        case WIFI_MODE_AP: return "ap";
        case WIFI_MODE_STA: return "sta";
        default: return "unknown";
    }
}

static const char *provisioning_state_to_string(provisioning_state_t state) {
    switch (state) {
        case PROVISIONING_STATE_UNPROVISIONED: return "unprovisioned";
        case PROVISIONING_STATE_ACTIVE: return "active";
        case PROVISIONING_STATE_CONNECTED: return "connected";
        default: return "unknown";
    }
}

static void app_main_log_snapshot(void) {
    device_status_snapshot_t snapshot;

    if (status_service_get_snapshot(&snapshot) != ERR_OK) {
        ESP_LOGW(TAG, "status snapshot unavailable");
        return;
    }

    ESP_LOGI(
        TAG,
        "heartbeat state=%s mcp=%s battery=%u%% wifi_mode=%s wifi_connected=%s ssid=%s ip=%s provisioning=%s display_busy=%s refresh_mode=%u refresh_ms=%" PRIu32,
        app_state_to_string(app_controller_get_state()),
        app_controller_is_mcp_enabled() ? "on" : "off",
        (unsigned int)snapshot.battery.percent,
        wifi_mode_to_string(snapshot.wifi.mode),
        snapshot.wifi.connected ? "yes" : "no",
        snapshot.wifi.ssid,
        snapshot.wifi.ip_address,
        provisioning_state_to_string(snapshot.provisioning.state),
        snapshot.display.busy ? "yes" : "no",
        (unsigned int)snapshot.display.last_refresh_mode,
        snapshot.display.last_refresh_elapsed_ms);
}

static void app_main_update_status_screen(bool force_refresh) {
    memset(&g_status_screen, 0, sizeof(g_status_screen));
    g_status_screen.app_state = app_controller_get_state();
    g_status_screen.mcp_enabled = app_controller_is_mcp_enabled();

    if (status_service_get_snapshot(&g_status_screen.snapshot) != ERR_OK) {
#ifdef ESP_PLATFORM
        ESP_LOGW(TAG, "status snapshot unavailable for screen render");
#endif
        return;
    }

    app_main_prepare_screen_compare_model(&g_status_screen);

    if (!force_refresh && g_status_screen_rendered &&
        memcmp(&g_status_screen, &g_last_rendered_status_screen, sizeof(g_status_screen)) == 0) {
        return;
    }

    if (render_service_render_status_screen(&g_status_screen, &g_status_screen_result) == ERR_OK) {
        g_last_rendered_status_screen = g_status_screen;
        g_status_screen_rendered = true;
#ifdef ESP_PLATFORM
        ESP_LOGI(TAG, "status screen rendered and refreshed");
#endif
    } else {
#ifdef ESP_PLATFORM
        ESP_LOGW(TAG, "status screen render failed");
#endif
    }
}
#endif

void app_main(void) {
#ifdef ESP_PLATFORM
    uint32_t heartbeat_tick = 0U;
    ESP_LOGI(TAG, "boot start");
#endif
    if (app_controller_init() != ERR_OK) {
#ifdef ESP_PLATFORM
        ESP_LOGE(TAG, "app_controller_init failed");
#endif
        return;
    }

#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "boot init complete");
#endif

    if (app_controller_run() != ERR_OK) {
#ifdef ESP_PLATFORM
        ESP_LOGE(TAG, "app_controller_run failed");
#endif
        return;
    }

#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "boot complete");
#endif

#ifdef ESP_PLATFORM
    app_main_update_status_screen(true);
    app_main_log_snapshot();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (app_controller_tick() != ERR_OK) {
            ESP_LOGW(TAG, "app_controller_tick reported an error");
        }
        app_main_update_status_screen(false);
        heartbeat_tick += 5000U;
        if (heartbeat_tick >= 30000U) {
            app_main_log_snapshot();
            heartbeat_tick = 0U;
        }
    }
#endif
}
