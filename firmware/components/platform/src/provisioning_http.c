#include "provisioning_http.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_http_server.h"
#endif

#define PROVISIONING_HTTP_MAX_BODY_LEN 640U

static provisioning_http_handlers_t g_handlers;
static bool g_initialized = false;
static bool g_running = false;

#ifdef ESP_PLATFORM
static httpd_handle_t g_server = NULL;

static const char *g_provisioning_page =
    "<!doctype html>"
    "<html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>reTerminal E1001 Setup</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;margin:0;background:#f3f0e8;color:#1c1b18;}"
    ".wrap{max-width:640px;margin:0 auto;padding:24px;}"
    "h1{margin:0 0 8px;font-size:28px;}"
    "p{line-height:1.5;}"
    "label{display:block;margin:14px 0 6px;font-weight:600;}"
    "input{width:100%;box-sizing:border-box;padding:12px;font-size:16px;border:1px solid #b9b0a0;border-radius:10px;background:#fffdf8;}"
    "button{margin-top:18px;padding:12px 16px;font-size:16px;border:0;border-radius:10px;background:#1f6f5f;color:#fff;}"
    ".note{margin-top:16px;padding:14px;border-radius:10px;background:#fff8de;color:#5d4b18;}"
    ".ok{background:#e8f5ef;color:#124d35;}"
    ".err{background:#fdecea;color:#7a1d1d;}"
    "</style></head><body><div class=\"wrap\">"
    "<h1>Device Setup</h1>"
    "<p>Enter the Wi-Fi network and device identity you want this reTerminal E1001 to use.</p>"
    "<form id=\"setup-form\">"
    "<label for=\"ssid\">Wi-Fi SSID</label><input id=\"ssid\" name=\"ssid\" maxlength=\"63\" required>"
    "<label for=\"password\">Wi-Fi Password</label><input id=\"password\" name=\"password\" type=\"password\" maxlength=\"127\">"
    "<label for=\"device_name\">Device Name</label><input id=\"device_name\" name=\"device_name\" maxlength=\"63\" required>"
    "<label for=\"auth_token\">Auth Token</label><input id=\"auth_token\" name=\"auth_token\" maxlength=\"127\">"
    "<button type=\"submit\">Save and Connect</button>"
    "</form><div id=\"result\" class=\"note\">Waiting for setup details.</div>"
    "<script>"
    "const form=document.getElementById('setup-form');"
    "const result=document.getElementById('result');"
    "form.addEventListener('submit',async(e)=>{e.preventDefault();"
    "result.className='note';result.textContent='Saving configuration and switching to Wi-Fi...';"
    "const body=new URLSearchParams(new FormData(form));"
    "try{const resp=await fetch('/api/provision',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});"
    "const data=await resp.json();"
    "result.className=resp.ok?'note ok':'note err';"
    "result.textContent=data.message||'Unexpected response.';"
    "}catch(err){result.className='note err';result.textContent='Setup request failed. Stay on this page and try again.';}});"
    "</script></div></body></html>";

static size_t provisioning_http_copy_value(const char *src, size_t src_len, char *dst, size_t dst_len) {
    size_t i;
    size_t written = 0U;

    if (!dst || dst_len == 0U) {
        return 0U;
    }

    for (i = 0U; i < src_len && written + 1U < dst_len; ++i) {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '+') {
            dst[written++] = ' ';
            continue;
        }

        if (ch == '%' && (i + 2U) < src_len && isxdigit((unsigned char)src[i + 1U]) && isxdigit((unsigned char)src[i + 2U])) {
            unsigned char hi = (unsigned char)src[i + 1U];
            unsigned char lo = (unsigned char)src[i + 2U];
            hi = (unsigned char)(isdigit(hi) ? (hi - '0') : (10 + (tolower(hi) - 'a')));
            lo = (unsigned char)(isdigit(lo) ? (lo - '0') : (10 + (tolower(lo) - 'a')));
            dst[written++] = (char)((hi << 4) | lo);
            i += 2U;
            continue;
        }

        dst[written++] = (char)ch;
    }

    dst[written] = '\0';
    return written;
}

static bool provisioning_http_get_form_value(const char *body, const char *key, char *dst, size_t dst_len) {
    const char *cursor = body;
    size_t key_len = strlen(key);

    if (!body || !key || !dst || dst_len == 0U) {
        return false;
    }

    dst[0] = '\0';
    while (*cursor != '\0') {
        const char *entry_end = strchr(cursor, '&');
        const char *equals = strchr(cursor, '=');
        size_t entry_len = entry_end ? (size_t)(entry_end - cursor) : strlen(cursor);

        if (equals && (size_t)(equals - cursor) == key_len && strncmp(cursor, key, key_len) == 0) {
            const char *value = equals + 1;
            size_t value_len = entry_len - (size_t)(value - cursor);
            (void)provisioning_http_copy_value(value, value_len, dst, dst_len);
            return true;
        }

        if (!entry_end) {
            break;
        }
        cursor = entry_end + 1;
    }

    return false;
}

static esp_err_t provisioning_http_send_json(httpd_req_t *req, int status_code, const char *json) {
    if (!req || !json) {
        return ESP_FAIL;
    }

    if (status_code >= 400) {
        httpd_resp_set_status(req, status_code == 400 ? "400 Bad Request" : "500 Internal Server Error");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t provisioning_http_index_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, g_provisioning_page);
}

static esp_err_t provisioning_http_submit_post(httpd_req_t *req) {
    char body[PROVISIONING_HTTP_MAX_BODY_LEN];
    provisioning_http_submission_t submission;
    int received;
    error_code_t err;

    if (!g_handlers.submit) {
        return provisioning_http_send_json(req, 500, "{\"ok\":false,\"message\":\"Provisioning handler unavailable.\"}");
    }

    if (req->content_len <= 0 || req->content_len >= (int)sizeof(body)) {
        return provisioning_http_send_json(req, 400, "{\"ok\":false,\"message\":\"Request body too large.\"}");
    }

    received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return provisioning_http_send_json(req, 400, "{\"ok\":false,\"message\":\"Failed to read request body.\"}");
    }
    body[received] = '\0';

    memset(&submission, 0, sizeof(submission));
    (void)provisioning_http_get_form_value(body, "ssid", submission.ssid, sizeof(submission.ssid));
    (void)provisioning_http_get_form_value(body, "password", submission.password, sizeof(submission.password));
    (void)provisioning_http_get_form_value(body, "device_name", submission.device_name, sizeof(submission.device_name));
    (void)provisioning_http_get_form_value(body, "auth_token", submission.auth_token, sizeof(submission.auth_token));

    err = g_handlers.submit(&submission);
    if (err != ERR_OK) {
        return provisioning_http_send_json(req, 400, "{\"ok\":false,\"message\":\"Invalid setup data. Check SSID and device name.\"}");
    }

    return provisioning_http_send_json(req, 200, "{\"ok\":true,\"message\":\"Configuration saved. The device is switching to your Wi-Fi network now.\"}");
}

static const httpd_uri_t g_index_get_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = provisioning_http_index_get,
    .user_ctx = NULL,
};

static const httpd_uri_t g_submit_post_uri = {
    .uri = "/api/provision",
    .method = HTTP_POST,
    .handler = provisioning_http_submit_post,
    .user_ctx = NULL,
};
#endif

error_code_t provisioning_http_init(void) {
    memset(&g_handlers, 0, sizeof(g_handlers));
    g_initialized = true;
    return ERR_OK;
}

error_code_t provisioning_http_start(const provisioning_http_handlers_t *handlers) {
    if (!handlers || !handlers->submit) {
        return ERR_INVALID_ARGS;
    }

    if (!g_initialized) {
        (void)provisioning_http_init();
    }

#ifdef ESP_PLATFORM
    if (!g_running) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = 80;
        config.max_uri_handlers = 4;
        config.stack_size = 8192;

        if (httpd_start(&g_server, &config) != ESP_OK) {
            return ERR_INTERNAL;
        }

        if (httpd_register_uri_handler(g_server, &g_index_get_uri) != ESP_OK ||
            httpd_register_uri_handler(g_server, &g_submit_post_uri) != ESP_OK) {
            httpd_stop(g_server);
            g_server = NULL;
            return ERR_INTERNAL;
        }
    }
#endif

    g_handlers = *handlers;
    g_running = true;
    return ERR_OK;
}

error_code_t provisioning_http_stop(void) {
#ifdef ESP_PLATFORM
    if (g_server) {
        (void)httpd_stop(g_server);
        g_server = NULL;
    }
#endif
    memset(&g_handlers, 0, sizeof(g_handlers));
    g_running = false;
    return ERR_OK;
}

bool provisioning_http_is_running(void) {
    return g_running;
}
