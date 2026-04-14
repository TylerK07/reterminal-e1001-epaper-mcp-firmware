#include "mcp_dispatch.h"
#include <stdio.h>
#include <string.h>
#include "asset_service.h"
#include "config_service.h"
#include "mcp_registry.h"
#include "network_service.h"
#include "render_service.h"
#include "status_service.h"

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
    char json[1024];
    uint32_t json_len = 0U;
    const mcp_tool_metadata_t *tool_meta;
    const mcp_resource_metadata_t *resource_meta;
    device_status_snapshot_t snapshot;
    wifi_scan_results_t scan_results;
    last_render_record_t last_render;

    if (!req || !out_resp) return ERR_INVALID_ARGS;
    memset(out_resp, 0, sizeof(*out_resp));

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
            if (status_service_get_snapshot_json(json, sizeof(json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode status snapshot.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://config") == 0) {
            if (config_service_get_redacted_json(json, sizeof(json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode config snapshot.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://storage/assets") == 0) {
            if (asset_service_get_list_json("/assets", json, sizeof(json), &json_len) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to encode asset list.", true);
                return ERR_OK;
            }
            mcp_dispatch_set_success(out_resp, req->target, json);
            return ERR_OK;
        }
        if (strcmp(req->target, "device://render/last-job") == 0) {
            if (render_service_get_last_render_record(&last_render) != ERR_OK) {
                mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get last render record.", true);
                return ERR_OK;
            }
            snprintf(
                json,
                sizeof(json),
                "{\"job_kind\":%u,\"display_refreshed\":%s,\"error_code\":\"%s\"}",
                (unsigned int)last_render.job_kind,
                last_render.display_refreshed ? "true" : "false",
                error_code_to_string(last_render.error_code));
            mcp_dispatch_set_success(out_resp, req->target, json);
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
        if (status_service_get_snapshot(&snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get device info.", true);
            return ERR_OK;
        }
        snprintf(
            json,
            sizeof(json),
            "{\"device_name\":\"%s\",\"model\":\"%s\",\"firmware_version\":\"%s\"}",
            snapshot.device.device_name,
            snapshot.device.model,
            snapshot.device.firmware_version);
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_battery_status") == 0) {
        if (status_service_get_snapshot(&snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get battery status.", true);
            return ERR_OK;
        }
        snprintf(
            json,
            sizeof(json),
            "{\"percent\":%u,\"voltage_mv\":%u,\"low_battery\":%s}",
            (unsigned int)snapshot.battery.percent,
            (unsigned int)snapshot.battery.voltage_mv,
            snapshot.battery.low_battery ? "true" : "false");
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_wifi_status") == 0) {
        if (status_service_get_snapshot(&snapshot) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get Wi-Fi status.", true);
            return ERR_OK;
        }
        snprintf(
            json,
            sizeof(json),
            "{\"connected\":%s,\"ssid\":\"%s\",\"ip_address\":\"%s\",\"hostname\":\"%s\"}",
            snapshot.wifi.connected ? "true" : "false",
            snapshot.wifi.ssid,
            snapshot.wifi.ip_address,
            snapshot.wifi.hostname);
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    if (strcmp(req->target, "scan_wifi") == 0) {
        if (network_service_scan(&scan_results, 5000U) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Wi-Fi scan failed.", true);
            return ERR_OK;
        }
        snprintf(json, sizeof(json), "{\"count\":%u}", (unsigned int)scan_results.count);
        mcp_dispatch_set_success(out_resp, req->target, json);
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
        if (asset_service_get_list_json("/assets", json, sizeof(json), &json_len) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to list assets.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    if (strcmp(req->target, "get_config") == 0) {
        if (config_service_get_redacted_json(json, sizeof(json), &json_len) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to get config.", true);
            return ERR_OK;
        }
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    if (strcmp(req->target, "refresh_display") == 0) {
        render_result_t result;
        if (render_service_refresh(DISPLAY_REFRESH_PARTIAL, NULL, &result) != ERR_OK) {
            mcp_dispatch_set_error(out_resp, ERR_INTERNAL, "Failed to refresh display.", true);
            return ERR_OK;
        }
        snprintf(
            json,
            sizeof(json),
            "{\"display_refreshed\":%s,\"refresh_mode\":%u,\"elapsed_ms\":%u}",
            result.display_refreshed ? "true" : "false",
            (unsigned int)result.refresh_mode,
            (unsigned int)result.elapsed_ms);
        mcp_dispatch_set_success(out_resp, req->target, json);
        return ERR_OK;
    }

    mcp_dispatch_set_error(out_resp, ERR_UNSUPPORTED, "Tool stub recognized but not implemented yet.", false);
    return ERR_OK;
}
