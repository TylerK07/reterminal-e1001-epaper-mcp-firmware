#include "render_service.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "display_epaper.h"
#include "power.h"

static last_render_record_t g_last_render;
static display_text_draw_req_t g_text_draw_req;
static rect_u16_t g_text_draw_region;

static void render_service_store_record(render_job_kind_t job_kind, const render_result_t *result, error_code_t error_code) {
    memset(&g_last_render, 0, sizeof(g_last_render));
    g_last_render.job_kind = job_kind;
    g_last_render.error_code = error_code;
    if (result) {
        g_last_render.framebuffer_updated = result->framebuffer_updated;
        g_last_render.display_refreshed = result->display_refreshed;
        g_last_render.refresh_mode = result->refresh_mode;
        g_last_render.affected_region = result->affected_region;
        g_last_render.elapsed_ms = result->elapsed_ms;
    }
}

static error_code_t render_service_render_builtin_pattern(render_job_kind_t job_kind, render_result_t *out_result) {
    rect_u16_t region;
    error_code_t err;

    if (!out_result) {
        return ERR_INVALID_ARGS;
    }

    memset(out_result, 0, sizeof(*out_result));
    err = display_draw_test_pattern(&region);
    if (err != ERR_OK) {
        render_service_store_record(job_kind, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region = region;
    err = render_service_refresh(DISPLAY_REFRESH_FULL, NULL, out_result);
    if (err != ERR_OK) {
        render_service_store_record(job_kind, out_result, err);
        return err;
    }

    render_service_store_record(job_kind, out_result, ERR_OK);
    return ERR_OK;
}

static uint16_t render_service_font_size_to_pixels(uint16_t requested_size) {
    if (requested_size >= 32U) {
        return 32U;
    }
    if (requested_size >= 24U) {
        return 24U;
    }
    if (requested_size >= 16U) {
        return 16U;
    }
    return 12U;
}

static void render_service_uppercase_copy(char *dst, size_t dst_len, const char *src) {
    size_t i;

    if (!dst || dst_len == 0U) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i + 1U < dst_len && src[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (char)toupper(ch);
    }
    dst[i] = '\0';
}

static error_code_t render_service_draw_line(const char *text, uint16_t x, uint16_t y, uint16_t size) {
    memset(&g_text_draw_req, 0, sizeof(g_text_draw_req));
    strncpy(g_text_draw_req.text, text, sizeof(g_text_draw_req.text) - 1U);
    strncpy(g_text_draw_req.font, "builtin-5x7", sizeof(g_text_draw_req.font) - 1U);
    strncpy(g_text_draw_req.align, "left", sizeof(g_text_draw_req.align) - 1U);
    g_text_draw_req.x = x;
    g_text_draw_req.y = y;
    g_text_draw_req.size = render_service_font_size_to_pixels(size);
    return display_draw_text(&g_text_draw_req, &g_text_draw_region);
}

static const char *render_service_app_state_label(app_state_t state) {
    switch (state) {
        case STATE_BOOTING: return "BOOTING";
        case STATE_UNPROVISIONED: return "UNPROVISIONED";
        case STATE_PROVISIONING: return "PROVISIONING";
        case STATE_CONNECTING: return "CONNECTING";
        case STATE_CONNECTED_IDLE: return "CONNECTED IDLE";
        case STATE_CONNECTED_ACTIVE: return "CONNECTED ACTIVE";
        case STATE_SLEEP_PREP: return "SLEEP PREP";
        case STATE_SLEEPING: return "SLEEPING";
        case STATE_ERROR_RECOVERY: return "ERROR RECOVERY";
        default: return "UNKNOWN";
    }
}

static const char *render_service_wifi_mode_label(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_OFF: return "OFF";
        case WIFI_MODE_AP: return "ACCESS POINT";
        case WIFI_MODE_STA: return "STATION";
        default: return "UNKNOWN";
    }
}

static const char *render_service_provisioning_state_label(provisioning_state_t state) {
    switch (state) {
        case PROVISIONING_STATE_UNPROVISIONED: return "UNPROVISIONED";
        case PROVISIONING_STATE_ACTIVE: return "SETUP ACTIVE";
        case PROVISIONING_STATE_CONNECTED: return "CONFIGURED";
        default: return "UNKNOWN";
    }
}

static const char *render_service_power_policy_label(power_policy_t policy) {
    switch (policy) {
        case POWER_POLICY_NORMAL: return "NORMAL";
        case POWER_POLICY_RESTRICTED: return "RESTRICTED";
        case POWER_POLICY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

error_code_t render_service_init(void) {
    memset(&g_last_render, 0, sizeof(g_last_render));
    return ERR_OK;
}

error_code_t render_service_render_text(const render_text_req_t *req, render_result_t *out_result) {
    display_text_draw_req_t draw_req;
    rect_u16_t region;
    error_code_t err;

    if (!req || !out_result) return ERR_INVALID_ARGS;
    memset(out_result, 0, sizeof(*out_result));

    if (display_is_busy()) {
        render_service_store_record(RENDER_JOB_TEXT, out_result, ERR_BUSY);
        return ERR_BUSY;
    }

    memset(&draw_req, 0, sizeof(draw_req));
    strncpy(draw_req.text, req->text, sizeof(draw_req.text) - 1U);
    strncpy(draw_req.font, req->font, sizeof(draw_req.font) - 1U);
    strncpy(draw_req.align, req->align, sizeof(draw_req.align) - 1U);
    draw_req.x = req->x;
    draw_req.y = req->y;
    draw_req.size = req->size;

    err = display_draw_text(&draw_req, &region);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_TEXT, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region = region;
    if (req->commit) {
        err = render_service_refresh(req->refresh_mode, &region, out_result);
        if (err != ERR_OK) {
            render_service_store_record(RENDER_JOB_TEXT, out_result, err);
            return err;
        }
    }

    render_service_store_record(RENDER_JOB_TEXT, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_refresh(display_refresh_mode_t mode, const rect_u16_t *region, render_result_t *out_result) {
    power_policy_t policy;
    error_code_t err = ERR_OK;

    if (!out_result) return ERR_INVALID_ARGS;
    memset(out_result, 0, sizeof(*out_result));

    if (power_get_policy(&policy) != ERR_OK) {
        render_service_store_record(RENDER_JOB_REFRESH, out_result, ERR_INTERNAL);
        return ERR_INTERNAL;
    }
    if (mode == DISPLAY_REFRESH_FULL && !power_operation_allowed(policy, "display_refresh_full")) {
        render_service_store_record(RENDER_JOB_REFRESH, out_result, ERR_LOW_BATTERY);
        return ERR_LOW_BATTERY;
    }
    if (display_is_busy()) {
        render_service_store_record(RENDER_JOB_REFRESH, out_result, ERR_BUSY);
        return ERR_BUSY;
    }

    if (mode == DISPLAY_REFRESH_PARTIAL && region) {
        err = display_refresh_partial(region, &out_result->elapsed_ms);
    } else {
        err = display_refresh_full(&out_result->elapsed_ms);
        mode = DISPLAY_REFRESH_FULL;
    }

    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_REFRESH, out_result, err);
        return err;
    }

    out_result->display_refreshed = true;
    out_result->refresh_mode = mode;
    if (region) {
        out_result->affected_region = *region;
    }
    render_service_store_record(RENDER_JOB_REFRESH, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_render_bitmap(const render_bitmap_req_t *req, render_result_t *out_result) {
    display_bitmap_draw_req_t draw_req;
    rect_u16_t region;
    error_code_t err;

    if (!req || !out_result) {
        return ERR_INVALID_ARGS;
    }

    memset(out_result, 0, sizeof(*out_result));
    memset(&draw_req, 0, sizeof(draw_req));
    strncpy(draw_req.asset_path, req->asset_path, sizeof(draw_req.asset_path) - 1U);
    draw_req.x = req->x;
    draw_req.y = req->y;

    err = display_draw_bitmap(&draw_req, &region);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_BITMAP, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region = region;
    if (req->commit) {
        err = render_service_refresh(req->refresh_mode, &region, out_result);
        if (err != ERR_OK) {
            render_service_store_record(RENDER_JOB_BITMAP, out_result, err);
            return err;
        }
    }

    render_service_store_record(RENDER_JOB_BITMAP, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_render_layout(const render_layout_req_t *req, render_result_t *out_result) {
    display_layout_draw_req_t draw_req;
    rect_u16_t region;
    error_code_t err;

    if (!req || !out_result) {
        return ERR_INVALID_ARGS;
    }

    memset(out_result, 0, sizeof(*out_result));
    memset(&draw_req, 0, sizeof(draw_req));
    strncpy(draw_req.layout_name, req->layout_name, sizeof(draw_req.layout_name) - 1U);
    strncpy(draw_req.payload_json, req->payload_json, sizeof(draw_req.payload_json) - 1U);

    err = display_draw_layout(&draw_req, &region);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region = region;
    if (req->commit) {
        err = render_service_refresh(req->refresh_mode, &region, out_result);
        if (err != ERR_OK) {
            render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
            return err;
        }
    }

    render_service_store_record(RENDER_JOB_LAYOUT, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_render_boot_screen(render_result_t *out_result) {
    error_code_t err;

    if (!out_result) {
        return ERR_INVALID_ARGS;
    }

    memset(out_result, 0, sizeof(*out_result));
    err = display_clear_framebuffer(true);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    err = render_service_draw_line("RETERMINAL E1001", 32U, 32U, 32U);
    if (err == ERR_OK) {
        err = render_service_draw_line("BOOTING MCP FIRMWARE", 32U, 92U, 24U);
    }
    if (err == ERR_OK) {
        err = render_service_draw_line("DISPLAY AND PLATFORM BRING-UP", 32U, 140U, 16U);
    }
    if (err == ERR_OK) {
        err = render_service_draw_line("PLEASE WAIT...", 32U, 188U, 16U);
    }
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region.x = 0U;
    out_result->affected_region.y = 0U;
    out_result->affected_region.w = 800U;
    out_result->affected_region.h = 480U;
    err = render_service_refresh(DISPLAY_REFRESH_FULL, NULL, out_result);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    render_service_store_record(RENDER_JOB_LAYOUT, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_render_status_screen(const render_status_screen_t *screen, render_result_t *out_result) {
    char line[160];
    char scratch[96];
    const uint16_t left = 24U;
    uint16_t y = 20U;
    error_code_t err;

    if (!screen || !out_result) {
        return ERR_INVALID_ARGS;
    }

    memset(out_result, 0, sizeof(*out_result));
    err = display_clear_framebuffer(true);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    err = render_service_draw_line("RETERMINAL E1001", left, y, 24U);
    y += 34U;
    if (err == ERR_OK) {
        err = render_service_draw_line("MCP DEVICE STATUS", left, y, 12U);
    }
    y += 28U;
    if (err == ERR_OK) {
        err = render_service_draw_line("----------------------------------------", left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "DEVICE  %s", screen->snapshot.device.device_name);
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "STATE   %s", render_service_app_state_label(screen->app_state));
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "MCP     %s", screen->mcp_enabled ? "ENABLED" : "DISABLED");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "BATTERY %u%%  %s", (unsigned int)screen->snapshot.battery.percent, render_service_power_policy_label(screen->snapshot.battery.power_policy));
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "POWER   %s", screen->snapshot.battery.charging ? "CHARGING" : "BATTERY");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 32U;
    if (err == ERR_OK) {
        err = render_service_draw_line("NETWORK", left, y, 16U);
    }
    y += 28U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "MODE    %s", render_service_wifi_mode_label(screen->snapshot.wifi.mode));
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(
            line,
            sizeof(line),
            "WIFI    %s",
            screen->snapshot.wifi.connected ? screen->snapshot.wifi.ssid : "NOT CONNECTED");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "IP      %s", screen->snapshot.wifi.ip_address[0] ? screen->snapshot.wifi.ip_address : "N/A");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "RSSI    %d DBM", (int)screen->snapshot.wifi.rssi);
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 32U;
    if (err == ERR_OK) {
        err = render_service_draw_line("PROVISIONING", left, y, 16U);
    }
    y += 28U;
    if (err == ERR_OK) {
        snprintf(line, sizeof(line), "STATUS  %s", render_service_provisioning_state_label(screen->snapshot.provisioning.state));
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(
            line,
            sizeof(line),
            "SETUP   %s",
            screen->snapshot.provisioning.ap_active ? screen->snapshot.provisioning.ap_ssid : "INACTIVE");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }
    y += 24U;
    if (err == ERR_OK) {
        snprintf(
            line,
            sizeof(line),
            "PORTAL  %s",
            screen->snapshot.provisioning.ap_ip_address[0] ? screen->snapshot.provisioning.ap_ip_address : "N/A");
        render_service_uppercase_copy(scratch, sizeof(scratch), line);
        err = render_service_draw_line(scratch, left, y, 12U);
    }

    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    out_result->framebuffer_updated = true;
    out_result->affected_region.x = 0U;
    out_result->affected_region.y = 0U;
    out_result->affected_region.w = 800U;
    out_result->affected_region.h = 480U;
    err = render_service_refresh(DISPLAY_REFRESH_FULL, NULL, out_result);
    if (err != ERR_OK) {
        render_service_store_record(RENDER_JOB_LAYOUT, out_result, err);
        return err;
    }

    render_service_store_record(RENDER_JOB_LAYOUT, out_result, ERR_OK);
    return ERR_OK;
}

error_code_t render_service_render_test_pattern(render_result_t *out_result) {
    return render_service_render_builtin_pattern(RENDER_JOB_LAYOUT, out_result);
}

error_code_t render_service_get_last_render_record(last_render_record_t *out_record) {
    if (!out_record) {
        return ERR_INVALID_ARGS;
    }

    *out_record = g_last_render;
    return ERR_OK;
}
