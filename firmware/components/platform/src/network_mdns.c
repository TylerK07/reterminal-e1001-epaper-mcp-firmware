#include "network_mdns.h"
#include <stdbool.h>
#include <string.h>
#if defined(ESP_PLATFORM) && __has_include("mdns.h")
#include "mdns.h"
#define NETWORK_MDNS_AVAILABLE 1
#else
#define NETWORK_MDNS_AVAILABLE 0
#endif

#if NETWORK_MDNS_AVAILABLE
static bool g_mdns_ready = false;
static bool g_mdns_running = false;
#endif

error_code_t network_mdns_init(void) {
#if NETWORK_MDNS_AVAILABLE
    esp_err_t err;

    if (g_mdns_ready) {
        return ERR_OK;
    }

    err = mdns_init();
    if (err == ESP_ERR_INVALID_STATE) {
        g_mdns_ready = true;
        return ERR_OK;
    }
    if (err != ESP_OK) {
        return ERR_INTERNAL;
    }

    g_mdns_ready = true;
    return ERR_OK;
#else
    return ERR_OK;
#endif
}

error_code_t network_mdns_start(const char *hostname, const char *instance_name, uint16_t port) {
#if NETWORK_MDNS_AVAILABLE
    if (!hostname || hostname[0] == '\0' || !instance_name || instance_name[0] == '\0' || port == 0U) {
        return ERR_INVALID_ARGS;
    }
    if (network_mdns_init() != ERR_OK) {
        return ERR_INTERNAL;
    }

    if (mdns_hostname_set(hostname) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (mdns_instance_name_set(instance_name) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (mdns_service_add(NULL, "_http", "_tcp", port, NULL, 0) != ESP_OK) {
        if (mdns_service_port_set("_http", "_tcp", port) != ESP_OK) {
            return ERR_INTERNAL;
        }
    }
    if (mdns_service_txt_item_set("_http", "_tcp", "path", "/mcp") != ESP_OK) {
        return ERR_INTERNAL;
    }

    g_mdns_running = true;
    return ERR_OK;
#else
    (void)hostname;
    (void)instance_name;
    (void)port;
    return ERR_OK;
#endif
}

error_code_t network_mdns_stop(void) {
#if NETWORK_MDNS_AVAILABLE
    if (!g_mdns_ready || !g_mdns_running) {
        return ERR_OK;
    }

    if (mdns_service_remove("_http", "_tcp") != ESP_OK) {
        return ERR_INTERNAL;
    }
    g_mdns_running = false;
    return ERR_OK;
#else
    return ERR_OK;
#endif
}
