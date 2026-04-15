#include "mcp_dispatch.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "asset_service.h"
#include "config_service.h"
#include "mcp_registry.h"
#include "network_service.h"
#include "provisioning_service.h"
#include "render_service.h"
#include "status_service.h"

static char g_dispatch_json[1024];
static char g_dispatch_text[256];
static char g_dispatch_font[64];
static char g_dispatch_align[16];
static char g_dispatch_background[16];
static char g_dispatch_foreground[16];
static char g_dispatch_temp_value[16];
static char g_dispatch_humidity_value[16];
static device_status_snapshot_t g_dispatch_snapshot;
static environment_status_t g_dispatch_environment;
static wifi_scan_results_t g_dispatch_scan_results;
static last_render_record_t g_dispatch_last_render;
static render_result_t g_dispatch_render_result;
static render_text_req_t g_dispatch_render_text_req;
static rect_u16_t g_dispatch_region;

static bool mcp_dispatch_hex_digit(char ch, uint8_t *out_value) {
    if (!out_value) {
        return false;
    }
    if (ch >= '0' && ch <= '9') {
        *out_value = (uint8_t)(ch - '0');
        return true;
    }
    ch = (char)tolower((unsigned char)ch);
    if (ch >= 'a' && ch <= 'f') {
        *out_value = (uint8_t)(10 + (ch - 'a'));
        return true;
    }
    return false;
}

static bool mcp_dispatch_copy_decoded_value(const char *src, size_t src_len, char *dst, size_t dst_len) {
    size_t i;
    size_t written = 0U;

    if (!src || !dst || dst_len == 0U) {
        return false;
    }

    for (i = 0U; i < src_len && written + 1U < dst_len; ++i) {
        char ch = src[i];
        if (ch == '+') {
            dst[written++] = ' ';
            continue;
        }
        if (ch == '%' && (i + 2U) < src_len) {
            uint8_t hi;
            uint8_t lo;
            if (!mcp_dispatch_hex_digit(src[i + 1U], &hi) || !mcp_dispatch_hex_digit(src[i + 2U], &lo)) {
                return false;
            }
            dst[written++] = (char)((hi << 4) | lo);
            i += 2U;
            continue;
        }
        dst[written++] = ch;
    }

    dst[written] = '\0';
    return true;
}

static bool mcp_dispatch_get_arg_value(const char *args, const char *key, char *buffer, size_t buffer_len) {
    const char *cursor = args;
    size_t key_len;

    if (!args || !key || !buffer || buffer_len == 0U) {
        return false;
    }

    key_len = strlen(key);
    buffer[0] = '\0';
    while (*cursor != '\0') {
        const char *entry_end = strchr(cursor, '&');
        const char *equals = strchr(cursor, '=');
        size_t entry_len = entry_end ? (size_t)(entry_end - cursor) : strlen(cursor);

        if (equals && (size_t)(equals - cursor) == key_len && strncmp(cursor, key, key_len) == 0) {
            const char *value = equals + 1;
            size_t value_len = entry_len - (size_t)(value - cursor);
            return mcp_dispatch_copy_decoded_value(value, value_len, buffer, buffer_len);
        }

        if (!entry_end) {
            break;
        }
        cursor = entry_end + 1;
    }

    return false;
}

static bool mcp_dispatch_get_arg_u16(const char *args, const char *key, uint16_t *out_value) {
    char value[16];
    unsigned long parsed;
    char *end_ptr = NULL;

    if (!out_value || !mcp_dispatch_get_arg_value(args, key, value, sizeof(value))) {
        return false;
    }

    parsed = strtoul(value, &end_ptr, 10);
    if (!end_ptr || *end_ptr != '\0' || parsed > 65535UL) {
        return false;
    }

    *out_value = (uint16_t)parsed;
    return true;
}

static bool mcp_dispatch_get_arg_bool(const char *args, const char *key, bool *out_value) {
    char value[8];

    if (!out_value || !mcp_dispatch_get_arg_value(args, key, value, sizeof(value))) {
        return false;
    }

    if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
        *out_value = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static display_refresh_mode_t mcp_dispatch_parse_refresh_mode(const char *args, display_refresh_mode_t fallback_mode) {
    char value[16];

    if (!mcp_dispatch_get_arg_value(args, "mode", value, sizeof(value))) {
        return fallback_mode;
    }
    if (strcmp(value, "full") == 0) {
        return DISPLAY_REFRESH_FULL;
    }
    if (strcmp(value, "partial") == 0) {
        return DISPLAY_REFRESH_PARTIAL;
    }
    return fallback_mode;
}

static display_background_mode_t mcp_dispatch_parse_background_mode(
    const char *args,
    display_background_mode_t fallback_mode) {
    if (!mcp_dispatch_get_arg_value(args, "background", g_dispatch_background, sizeof(g_dispatch_background))) {
        return fallback_mode;
    }
    if (strcmp(g_dispatch_background, "transparent") == 0) {
        return DISPLAY_BACKGROUND_TRANSPARENT;
    }
    if (strcmp(g_dispatch_background, "white") == 0) {
        return DISPLAY_BACKGROUND_WHITE;
    }
    if (strcmp(g_dispatch_background, "black") == 0) {
        return DISPLAY_BACKGROUND_BLACK;
    }
    return fallback_mode;
}

static display_foreground_color_t mcp_dispatch_parse_foreground_color(
    const char *args,
    display_foreground_color_t fallback_color) {
    if (!mcp_dispatch_get_arg_value(args, "foreground", g_dispatch_foreground, sizeof(g_dispatch_foreground))) {
        return fallback_color;
    }
    if (strcmp(g_dispatch_foreground, "white") == 0) {
        return DISPLAY_FOREGROUND_WHITE;
    }
    if (strcmp(g_dispatch_foreground, "black") == 0) {
        return DISPLAY_FOREGROUND_BLACK;
    }
    return fallback_color;
}

static bool mcp_dispatch_has_arg(const char *args, const char *key) {
    char scratch[4];
    return mcp_dispatch_get_arg_value(args, key, scratch, sizeof(scratch));
}

static void mcp_dispatch_format_signed_centi(int32_t value_centi, char *buffer, size_t buffer_len) {
    uint32_t abs_value;

    if (!buffer || buffer_len == 0U) {
        return;
    }

    abs_value = (uint32_t)(value_centi < 0 ? -value_centi : value_centi);
    if (value_centi < 0) {
        snprintf(buffer, buffer_len, "-%u.%02u", (unsigned int)(abs_value / 100U), (unsigned int)(abs_value % 100U));
    } else {
        snprintf(buffer, buffer_len, "%u.%02u", (unsigned int)(abs_value / 100U), (unsigned int)(abs_value % 100U));
    }
}

static void mcp_dispatch_format_unsigned_centi(uint32_t value_centi, char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0U) {
        return;
    }

    snprintf(buffer, buffer_len, "%u.%02u", (unsigned int)(value_centi / 100U), (unsigned int)(value_centi % 100U));
}

static void mcp_dispatch_set_error(
    mcp_response_t *out_resp,
    error_code_t error_code,
    const char *message,
    bool retryable) {
    out_resp->ok = false;
    out_resp->error_code = error_code;
    out_resp->retryable = retryable;
    strncpy(out_resp->mime_type, "application/json", sizeof(out_resp->mime_type) - 1U);
    strncpy(out_resp->error_message, message, sizeof(out_resp->error_message) - 1U);
    snprintf(
        out_resp->body,
        sizeof(out_resp->body),
        "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"retryable\":%s}}",
        error_code_to_string(error_code),
        message,
        retryable ? "true" : "false");
}

static void mcp_dispatch_set_success(mcp_response_t *out_resp, const char *tool, const char *json_body) {
    out_resp->ok = true;
    out_resp->error_code = ERR_OK;
    strncpy(out_resp->mime_type, "application/json", sizeof(out_resp->mime_type) - 1U);
    snprintf(
        out_resp->body,
        sizeof(out_resp->body),
        "{\"ok\":true,\"tool\":\"%s\",\"result\":%s}",
        tool,
        json_body);
}

static bool mcp_dispatch_authorized(mcp_auth_level_t auth_level, const char *token) {
    if (auth_level == MCP_AUTH_NONE) {
        return true;
    }

    return config_service_token_matches(token);
}

error_code_t mcp_dispatch_init(void) { return ERR_OK; }

error_code_t mcp_dispatch_handle(const mcp_request_t *req, mcp_response_t *out_resp) {
    uint32_t json_len = 0U;
    const mcp_tool_metadata_t *tool_meta;
    const mcp_resource_metadata_t *resource_meta;

    if (!req || !out_resp) return ERR_INVALID_ARGS;
    memset(out_resp, 0, sizeof(*out_resp));
    memset(g_dispatch_json, 0, sizeof(g_dispatch_json));
    memset(g_dispatch_text, 0, sizeof(g_dispatch_text));
    memset(g_dispatch_font, 0, sizeof(g_dispatch_font));
    memset(g_dispatch_align, 0, sizeof(g_dispatch_align));
    memset(g_dispatch_background, 0, sizeof(g_dispatch_background));
    memset(g_dispatch_foreground, 0, sizeof(g_dispatch_foreground));
    memset(g_dispatch_temp_value, 0, sizeof(g_dispatch_temp_value));
    memset(g_dispatch_humidity_value, 0, sizeof(g_dispatch_humidity_value));
    memset(&g_dispatch_snapshot, 0, sizeof(g_dispatch_snapshot));
    memset(&g_dispatch_environment, 0, sizeof(g_dispatch_environment));
    memset(&g_dispatch_scan_results, 0, sizeof(g_dispatch_scan_results));
    memset(&g_dispatch_last_render, 0, sizeof(g_dispatch_last_render));
    memset(&g_dispatch_render_result, 0, sizeof(g_dispatch_render_result));
    memset(&g_dispatch_render_text_req, 0, sizeof(g_dispatch_render_text_req));
    memset(&g_dispatch_region, 0, sizeof(g_dispatch_region));

    if (req->request_method == MCP_REQUEST_METHOD_RESOURCE) {
        resource_meta = mcp_registry_find_resource(req->target);
        if (!resource_meta) {
            mcp_dispatch_set_error(out_resp, ERR_NOT_FOUND, "Unknown MCP resource.", false);
            return ERR_OK;
        }
        if (!mcp_dispatch_authorized(resource_meta->auth_level, req->token)) {
            mcp_dispatch_set_error(out_resp, ERR_UNAUTHORIZED, "Token required for resource access.", false);
            return ERR_OK;
        }

        if (strcmp(req->target, "device://status") == 0) {
            if (status_service_get_snapshot_json(g_dispatch_json, sizeof(g_dispatch_json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode status snapshot.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://config") == 0) {
            if (config_service_get_redacted_json(g_dispatch_json, sizeof(g_dispatch_json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode config snapshot.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://environment") == 0) {
            if (status_service_get_environment_status(&g_dispatch_environment) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get environment status.", true);
                return ERR_OK;
            }
            mcp_dispatch_format_signed_centi(
                g_dispatch_environment.temperature_centi_f,
                g_dispatch_temp_value,
                sizeof(g_dispatch_temp_value));
            mcp_dispatch_format_unsigned_centi(
                g_dispatch_environment.humidity_centi_pct,
                g_dispatch_humidity_value,
                sizeof(g_dispatch_humidity_value));
            snprintf(
                g_dispatch_json,
                sizeof(g_dispatch_json),
                "{\"sensor_present\":%s,\"reading_valid\":%s,\"temperature_f\":%s,\"humidity_percent\":%s}",
                g_dispatch_environment.sensor_present ? "true" : "false",
                g_dispatch_environment.reading_valid ? "true" : "false",
                g_dispatch_temp_value,
                g_dispatch_humidity_value);
            mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://storage/assets") == 0) {
            if (asset_service_get_list_json("/assets", g_dispatch_json, sizeof(g_dispatch_json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode asset list.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://render/last-job") == 0) {
            if (render_service_get_last_render_record(&g_dispatch_last_render) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get last render record.", true);
                return ERR_OK;
            }
            snprintf(
                g_dispatch_json,
                sizeof(g_dispatch_json),
                "{\"job_kind\":%u,\"display_refreshed\":%s,\"error_code\":\"%s\"}",
                (unsigned int)g_dispatch_last_render.job_kind,
                g_dispatch_last_render.display_refreshed ? "true" : "false",
                error_code_to_string(g_dispatch_last_render.error_code));
            mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
            return ERR_OK;
        }

        mcp_dispatch_set_error(out_resp, ERR_UNSUPPORTED, "Resource stub not implemented yet.", false);
        return ERR_OK;
    }

    tool_meta = mcp_registry_find_tool(req->target);
    if (!tool_meta) {
        mcp_dispatch_set_error(out_resp, ERR_NOT_FOUND, "Unknown MCP tool.", false);
        return ERR_OK;
    }
    if (!mcp_dispatch_authorized(tool_meta->auth_level, req->token)) {
        mcp_dispatch_set_error(out_resp, ERR_UNAUTHORIZED, "Token required for tool execution.", false);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_device_info") == 0) {
        if (status_service_get_snapshot(&g_dispatch_snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get device info.", true);
            return ERR_OK;
        }
        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"device_name\":\"%s\",\"model\":\"%s\",\"firmware_version\":\"%s\"}",
            g_dispatch_snapshot.device.device_name,
            g_dispatch_snapshot.device.model,
            g_dispatch_snapshot.device.firmware_version);
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_battery_status") == 0) {
        if (status_service_get_snapshot(&g_dispatch_snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get battery status.", true);
            return ERR_OK;
        }
        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"percent\":%u,\"voltage_mv\":%u,\"low_battery\":%s}",
            (unsigned int)g_dispatch_snapshot.battery.percent,
            (unsigned int)g_dispatch_snapshot.battery.voltage_mv,
            g_dispatch_snapshot.battery.low_battery ? "true" : "false");
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_wifi_status") == 0) {
        if (status_service_get_snapshot(&g_dispatch_snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get Wi-Fi status.", true);
            return ERR_OK;
        }
        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"connected\":%s,\"ssid\":\"%s\",\"ip_address\":\"%s\",\"hostname\":\"%s\"}",
            g_dispatch_snapshot.wifi.connected ? "true" : "false",
            g_dispatch_snapshot.wifi.ssid,
            g_dispatch_snapshot.wifi.ip_address,
            g_dispatch_snapshot.wifi.hostname);
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_environment_status") == 0) {
        if (status_service_get_environment_status(&g_dispatch_environment) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get environment status.", true);
            return ERR_OK;
        }
        mcp_dispatch_format_signed_centi(
            g_dispatch_environment.temperature_centi_f,
            g_dispatch_temp_value,
            sizeof(g_dispatch_temp_value));
        mcp_dispatch_format_unsigned_centi(
            g_dispatch_environment.humidity_centi_pct,
            g_dispatch_humidity_value,
            sizeof(g_dispatch_humidity_value));
        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"sensor_present\":%s,\"reading_valid\":%s,\"temperature_f\":%s,\"humidity_percent\":%s}",
            g_dispatch_environment.sensor_present ? "true" : "false",
            g_dispatch_environment.reading_valid ? "true" : "false",
            g_dispatch_temp_value,
            g_dispatch_humidity_value);
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "scan_wifi") == 0) {
        if (network_service_scan(&g_dispatch_scan_results, 5000U) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Wi-Fi scan failed.", true);
            return ERR_OK;
        }
        snprintf(g_dispatch_json, sizeof(g_dispatch_json), "{\"count\":%u}", (unsigned int)g_dispatch_scan_results.count);
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "disconnect_wifi") == 0) {
        if (network_service_disconnect() != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to disconnect Wi-Fi.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, "{\"disconnected\":true}");
        return ERR_OK;
    }

    if (strcmp(req->target, "list_assets") == 0) {
        if (asset_service_get_list_json("/assets", g_dispatch_json, sizeof(g_dispatch_json), &json_len) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to list assets.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_config") == 0) {
        if (config_service_get_redacted_json(g_dispatch_json, sizeof(g_dispatch_json), &json_len) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get config.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "refresh_display") == 0) {
        const rect_u16_t *region_ptr = NULL;
        display_refresh_mode_t refresh_mode = mcp_dispatch_parse_refresh_mode(req->args_json, DISPLAY_REFRESH_PARTIAL);

        memset(&g_dispatch_region, 0, sizeof(g_dispatch_region));
        if (refresh_mode == DISPLAY_REFRESH_PARTIAL &&
            mcp_dispatch_get_arg_u16(req->args_json, "x", &g_dispatch_region.x) &&
            mcp_dispatch_get_arg_u16(req->args_json, "y", &g_dispatch_region.y) &&
            mcp_dispatch_get_arg_u16(req->args_json, "w", &g_dispatch_region.w) &&
            mcp_dispatch_get_arg_u16(req->args_json, "h", &g_dispatch_region.h) &&
            g_dispatch_region.w > 0U && g_dispatch_region.h > 0U) {
            region_ptr = &g_dispatch_region;
        } else if (refresh_mode == DISPLAY_REFRESH_PARTIAL) {
            refresh_mode = DISPLAY_REFRESH_FULL;
        }

        if (render_service_refresh(refresh_mode, region_ptr, &g_dispatch_render_result) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to refresh display.", true);
            return ERR_OK;
        }
        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"display_refreshed\":%s,\"refresh_mode\":%u,\"elapsed_ms\":%u}",
            g_dispatch_render_result.display_refreshed ? "true" : "false",
            (unsigned int)g_dispatch_render_result.refresh_mode,
            (unsigned int)g_dispatch_render_result.elapsed_ms);
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "clear_region") == 0) {
        bool black = false;
        bool commit = true;
        display_refresh_mode_t refresh_mode = DISPLAY_REFRESH_PARTIAL;

        memset(&g_dispatch_region, 0, sizeof(g_dispatch_region));
        if (!mcp_dispatch_get_arg_u16(req->args_json, "x", &g_dispatch_region.x) ||
            !mcp_dispatch_get_arg_u16(req->args_json, "y", &g_dispatch_region.y) ||
            !mcp_dispatch_get_arg_u16(req->args_json, "w", &g_dispatch_region.w) ||
            !mcp_dispatch_get_arg_u16(req->args_json, "h", &g_dispatch_region.h) ||
            g_dispatch_region.w == 0U ||
            g_dispatch_region.h == 0U) {
            mcp_dispatch_set_error(out_resp, ERR_INVALID_ARGS, "x, y, w, and h are required.", false);
            return ERR_OK;
        }

        (void)mcp_dispatch_get_arg_bool(req->args_json, "commit", &commit);
        refresh_mode = mcp_dispatch_parse_refresh_mode(req->args_json, refresh_mode);
        switch (mcp_dispatch_parse_background_mode(req->args_json, DISPLAY_BACKGROUND_WHITE)) {
            case DISPLAY_BACKGROUND_BLACK:
                black = true;
                break;
            case DISPLAY_BACKGROUND_WHITE:
            case DISPLAY_BACKGROUND_TRANSPARENT:
            default:
                black = false;
                break;
        }

        if (render_service_clear_region(&g_dispatch_region, black, commit, refresh_mode, &g_dispatch_render_result) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to clear region.", true);
            return ERR_OK;
        }

        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"framebuffer_updated\":%s,\"display_refreshed\":%s,\"background\":\"%s\"}",
            g_dispatch_render_result.framebuffer_updated ? "true" : "false",
            g_dispatch_render_result.display_refreshed ? "true" : "false",
            black ? "black" : "white");
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "render_text") == 0) {
        memset(&g_dispatch_render_text_req, 0, sizeof(g_dispatch_render_text_req));
        memset(g_dispatch_text, 0, sizeof(g_dispatch_text));
        memset(g_dispatch_font, 0, sizeof(g_dispatch_font));
        memset(g_dispatch_align, 0, sizeof(g_dispatch_align));

        if (!mcp_dispatch_get_arg_value(req->args_json, "text", g_dispatch_text, sizeof(g_dispatch_text)) || g_dispatch_text[0] == '\0') {
            mcp_dispatch_set_error(out_resp, ERR_INVALID_ARGS, "Text is required.", false);
            return ERR_OK;
        }

        strncpy(g_dispatch_render_text_req.text, g_dispatch_text, sizeof(g_dispatch_render_text_req.text) - 1U);
        g_dispatch_render_text_req.x = 24U;
        g_dispatch_render_text_req.y = 24U;
        g_dispatch_render_text_req.size = 24U;
        g_dispatch_render_text_req.commit = true;
        g_dispatch_render_text_req.refresh_mode = DISPLAY_REFRESH_PARTIAL;
        g_dispatch_render_text_req.foreground_color = DISPLAY_FOREGROUND_BLACK;
        g_dispatch_render_text_req.background_mode = DISPLAY_BACKGROUND_TRANSPARENT;
        strncpy(g_dispatch_render_text_req.font, "builtin-5x7", sizeof(g_dispatch_render_text_req.font) - 1U);
        strncpy(g_dispatch_render_text_req.align, "left", sizeof(g_dispatch_render_text_req.align) - 1U);

        (void)mcp_dispatch_get_arg_u16(req->args_json, "x", &g_dispatch_render_text_req.x);
        (void)mcp_dispatch_get_arg_u16(req->args_json, "y", &g_dispatch_render_text_req.y);
        (void)mcp_dispatch_get_arg_u16(req->args_json, "size", &g_dispatch_render_text_req.size);
        (void)mcp_dispatch_get_arg_bool(req->args_json, "commit", &g_dispatch_render_text_req.commit);
        g_dispatch_render_text_req.refresh_mode = mcp_dispatch_parse_refresh_mode(req->args_json, g_dispatch_render_text_req.refresh_mode);
        g_dispatch_render_text_req.background_mode =
            mcp_dispatch_parse_background_mode(req->args_json, g_dispatch_render_text_req.background_mode);
        if (mcp_dispatch_has_arg(req->args_json, "foreground")) {
            g_dispatch_render_text_req.foreground_color =
                mcp_dispatch_parse_foreground_color(req->args_json, g_dispatch_render_text_req.foreground_color);
        } else if (g_dispatch_render_text_req.background_mode == DISPLAY_BACKGROUND_BLACK) {
            g_dispatch_render_text_req.foreground_color = DISPLAY_FOREGROUND_WHITE;
        } else {
            g_dispatch_render_text_req.foreground_color = DISPLAY_FOREGROUND_BLACK;
        }

        if (mcp_dispatch_get_arg_value(req->args_json, "font", g_dispatch_font, sizeof(g_dispatch_font)) && g_dispatch_font[0] != '\0') {
            strncpy(g_dispatch_render_text_req.font, g_dispatch_font, sizeof(g_dispatch_render_text_req.font) - 1U);
        }
        if (mcp_dispatch_get_arg_value(req->args_json, "align", g_dispatch_align, sizeof(g_dispatch_align)) && g_dispatch_align[0] != '\0') {
            strncpy(g_dispatch_render_text_req.align, g_dispatch_align, sizeof(g_dispatch_render_text_req.align) - 1U);
        }

        if (render_service_render_text(&g_dispatch_render_text_req, &g_dispatch_render_result) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to render text.", true);
            return ERR_OK;
        }

        snprintf(
            g_dispatch_json,
            sizeof(g_dispatch_json),
            "{\"framebuffer_updated\":%s,\"display_refreshed\":%s,\"refresh_mode\":%u,\"x\":%u,\"y\":%u,\"foreground\":\"%s\",\"background\":\"%s\"}",
            g_dispatch_render_result.framebuffer_updated ? "true" : "false",
            g_dispatch_render_result.display_refreshed ? "true" : "false",
            (unsigned int)g_dispatch_render_result.refresh_mode,
            (unsigned int)g_dispatch_render_text_req.x,
            (unsigned int)g_dispatch_render_text_req.y,
            g_dispatch_render_text_req.foreground_color == DISPLAY_FOREGROUND_WHITE ? "white" : "black",
            g_dispatch_render_text_req.background_mode == DISPLAY_BACKGROUND_BLACK ? "black" :
            g_dispatch_render_text_req.background_mode == DISPLAY_BACKGROUND_WHITE ? "white" : "transparent");
        mcp_dispatch_set_success(out_resp, req->target, g_dispatch_json);
        return ERR_OK;
    }

    if (strcmp(req->target, "reset_network") == 0) {
        if (provisioning_service_request_reset() != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to reset saved network configuration.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, "{\"reset\":true,\"reprovision_required\":true}");
        return ERR_OK;
    }

    mcp_dispatch_set_error(out_resp, ERR_UNSUPPORTED, "Tool stub recognized but not implemented yet.", false);
    return ERR_OK;
}
