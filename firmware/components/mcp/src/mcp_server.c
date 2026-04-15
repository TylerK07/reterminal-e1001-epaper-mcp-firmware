#include "mcp_server.h"
#include <string.h>
#include "mcp_dispatch.h"
#ifdef ESP_PLATFORM
#include "esp_http_server.h"
#endif

static mcp_server_status_t g_status;

#ifdef ESP_PLATFORM
static httpd_handle_t g_mcp_http_server = NULL;
static mcp_request_t g_mcp_request;
static mcp_response_t g_mcp_response;
static char g_mcp_token[160];
static char g_mcp_args[512];

static bool mcp_server_decode_component(const char *src, size_t src_len, char *dst, size_t dst_len) {
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
            char hi = src[i + 1U];
            char lo = src[i + 2U];
            uint8_t hi_val;
            uint8_t lo_val;

            if (!(((hi >= '0' && hi <= '9') || (hi >= 'a' && hi <= 'f') || (hi >= 'A' && hi <= 'F')) &&
                  ((lo >= '0' && lo <= '9') || (lo >= 'a' && lo <= 'f') || (lo >= 'A' && lo <= 'F')))) {
                return false;
            }

            hi_val = (uint8_t)((hi <= '9') ? (hi - '0') : (10 + ((hi | 32) - 'a')));
            lo_val = (uint8_t)((lo <= '9') ? (lo - '0') : (10 + ((lo | 32) - 'a')));
            dst[written++] = (char)((hi_val << 4) | lo_val);
            i += 2U;
            continue;
        }

        dst[written++] = ch;
    }

    dst[written] = '\0';
    return true;
}

static bool mcp_server_query_value(httpd_req_t *req, const char *key, char *buffer, size_t buffer_len) {
    char query[256];
    char raw_value[192];
    esp_err_t err;

    if (!req || !key || !buffer || buffer_len == 0U) {
        return false;
    }

    buffer[0] = '\0';
    if (httpd_req_get_url_query_len(req) <= 0) {
        return false;
    }

    if ((size_t)httpd_req_get_url_query_len(req) + 1U > sizeof(query)) {
        return false;
    }

    err = httpd_req_get_url_query_str(req, query, sizeof(query));
    if (err != ESP_OK) {
        return false;
    }
    err = httpd_query_key_value(query, key, raw_value, sizeof(raw_value));
    if (err != ESP_OK) {
        return false;
    }

    return mcp_server_decode_component(raw_value, strnlen(raw_value, sizeof(raw_value)), buffer, buffer_len);
}

static const char *mcp_server_token_from_request(httpd_req_t *req, char *buffer, size_t buffer_len) {
    if (!req || !buffer || buffer_len == 0U) {
        return NULL;
    }

    buffer[0] = '\0';
    if (httpd_req_get_hdr_value_str(req, "X-MCP-Token", buffer, buffer_len) == ESP_OK && buffer[0] != '\0') {
        return buffer;
    }
    if (mcp_server_query_value(req, "token", buffer, buffer_len) && buffer[0] != '\0') {
        return buffer;
    }
    return NULL;
}

static esp_err_t mcp_server_send_dispatch(httpd_req_t *req, mcp_request_method_t method, const char *target) {
    error_code_t err;
    int query_len;

    memset(&g_mcp_request, 0, sizeof(g_mcp_request));
    memset(&g_mcp_response, 0, sizeof(g_mcp_response));
    memset(g_mcp_token, 0, sizeof(g_mcp_token));
    memset(g_mcp_args, 0, sizeof(g_mcp_args));
    g_mcp_request.request_method = method;
    strncpy(g_mcp_request.target, target, sizeof(g_mcp_request.target) - 1U);
    g_mcp_request.token = mcp_server_token_from_request(req, g_mcp_token, sizeof(g_mcp_token));
    query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0 && (size_t)query_len + 1U < sizeof(g_mcp_args) &&
        httpd_req_get_url_query_str(req, g_mcp_args, sizeof(g_mcp_args)) == ESP_OK) {
        g_mcp_request.args_json = g_mcp_args;
    }

    err = mcp_dispatch_handle(&g_mcp_request, &g_mcp_response);
    if (err != ERR_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":{\"code\":\"ERR_INTERNAL\",\"message\":\"Dispatch failed.\",\"retryable\":true}}");
    }

    httpd_resp_set_type(req, g_mcp_response.mime_type[0] ? g_mcp_response.mime_type : "application/json");
    if (!g_mcp_response.ok) {
        httpd_resp_set_status(req, g_mcp_response.error_code == ERR_UNAUTHORIZED ? "401 Unauthorized" : "400 Bad Request");
    }
    return httpd_resp_sendstr(req, g_mcp_response.body);
}

static esp_err_t mcp_server_health_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"service\":\"mcp\",\"status\":\"running\"}");
}

static esp_err_t mcp_server_tool_get(httpd_req_t *req) {
    char name[128];

    if (!mcp_server_query_value(req, "name", name, sizeof(name)) || name[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":{\"code\":\"ERR_INVALID_ARGS\",\"message\":\"Tool name is required.\",\"retryable\":false}}");
    }

    return mcp_server_send_dispatch(req, MCP_REQUEST_METHOD_TOOL, name);
}

static esp_err_t mcp_server_resource_get(httpd_req_t *req) {
    char uri[160];

    if (!mcp_server_query_value(req, "uri", uri, sizeof(uri)) || uri[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":{\"code\":\"ERR_INVALID_ARGS\",\"message\":\"Resource URI is required.\",\"retryable\":false}}");
    }

    return mcp_server_send_dispatch(req, MCP_REQUEST_METHOD_RESOURCE, uri);
}

static const httpd_uri_t g_mcp_health_uri = {
    .uri = "/health",
    .method = HTTP_GET,
    .handler = mcp_server_health_get,
    .user_ctx = NULL,
};

static const httpd_uri_t g_mcp_tool_uri = {
    .uri = "/mcp/tool",
    .method = HTTP_GET,
    .handler = mcp_server_tool_get,
    .user_ctx = NULL,
};

static const httpd_uri_t g_mcp_resource_uri = {
    .uri = "/mcp/resource",
    .method = HTTP_GET,
    .handler = mcp_server_resource_get,
    .user_ctx = NULL,
};
#endif

error_code_t mcp_server_init(void) {
    g_status.port = 0U;
    g_status.running = false;
    return ERR_OK;
}

error_code_t mcp_server_start(uint16_t port) {
    if (port == 0U) {
        return ERR_INVALID_ARGS;
    }

#ifdef ESP_PLATFORM
    if (!g_status.running) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = port;
        config.max_uri_handlers = 6;
        config.stack_size = 10240;

        if (httpd_start(&g_mcp_http_server, &config) != ESP_OK) {
            return ERR_INTERNAL;
        }
        if (httpd_register_uri_handler(g_mcp_http_server, &g_mcp_health_uri) != ESP_OK ||
            httpd_register_uri_handler(g_mcp_http_server, &g_mcp_tool_uri) != ESP_OK ||
            httpd_register_uri_handler(g_mcp_http_server, &g_mcp_resource_uri) != ESP_OK) {
            httpd_stop(g_mcp_http_server);
            g_mcp_http_server = NULL;
            return ERR_INTERNAL;
        }
    }
#endif

    g_status.port = port;
    g_status.running = true;
    return ERR_OK;
}

error_code_t mcp_server_stop(void) {
#ifdef ESP_PLATFORM
    if (g_mcp_http_server) {
        (void)httpd_stop(g_mcp_http_server);
        g_mcp_http_server = NULL;
    }
#endif
    g_status.running = false;
    return ERR_OK;
}

error_code_t mcp_server_get_status(mcp_server_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    *out_status = g_status;
    return ERR_OK;
}
