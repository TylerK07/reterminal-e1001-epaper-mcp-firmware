#include "wifi.h"
#include <string.h>
#include "board.h"
#ifdef ESP_PLATFORM
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#define wifi_mode_t esp_idf_wifi_mode_t
#define wifi_ap_config_t esp_idf_wifi_ap_config_t
#define WIFI_MODE_NULL ESP_IDF_WIFI_MODE_NULL
#define WIFI_MODE_STA ESP_IDF_WIFI_MODE_STA
#define WIFI_MODE_AP ESP_IDF_WIFI_MODE_AP
#define WIFI_MODE_APSTA ESP_IDF_WIFI_MODE_APSTA
#define WIFI_MODE_MAX ESP_IDF_WIFI_MODE_MAX
#include "esp_wifi.h"
#undef WIFI_MODE_MAX
#undef WIFI_MODE_APSTA
#undef WIFI_MODE_AP
#undef WIFI_MODE_STA
#undef WIFI_MODE_NULL
#undef wifi_ap_config_t
#undef wifi_mode_t
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"
#endif

static platform_wifi_status_t g_status;
static board_pin_map_t g_pin_map;

#ifdef ESP_PLATFORM
static esp_netif_t *g_sta_netif = NULL;
static esp_netif_t *g_ap_netif = NULL;
static EventGroupHandle_t g_wifi_events = NULL;
static bool g_wifi_ready = false;
static bool g_sta_netif_up = false;
static bool g_ap_netif_up = false;
static char g_pending_hostname[64];

enum {
    WIFI_EVENT_STA_CONNECTED_BIT = BIT0,
    WIFI_EVENT_STA_FAILED_BIT = BIT1,
    WIFI_EVENT_SCAN_DONE_BIT = BIT2
};

static error_code_t wifi_format_ip_address(const esp_netif_ip_info_t *ip_info, char *buffer, uint32_t buffer_len) {
    if (!ip_info || !buffer || buffer_len == 0U) {
        return ERR_INVALID_ARGS;
    }

    if (ip4addr_ntoa_r((const ip4_addr_t *)&ip_info->ip, buffer, (int)buffer_len) == NULL) {
        buffer[0] = '\0';
        return ERR_INTERNAL;
    }

    return ERR_OK;
}

static void wifi_reset_status_runtime(void) {
    g_status.connected = false;
    g_status.ip_address[0] = '\0';
    g_status.rssi = 0;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                break;
            case WIFI_EVENT_STA_CONNECTED:
                g_status.mode = WIFI_MODE_STA;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                g_status.connected = false;
                g_sta_netif_up = false;
                g_status.ip_address[0] = '\0';
                xEventGroupSetBits(g_wifi_events, WIFI_EVENT_STA_FAILED_BIT);
                break;
            case WIFI_EVENT_SCAN_DONE:
                xEventGroupSetBits(g_wifi_events, WIFI_EVENT_SCAN_DONE_BIT);
                break;
            case WIFI_EVENT_AP_START:
                g_ap_netif_up = true;
                g_status.mode = WIFI_MODE_AP;
                break;
            case WIFI_EVENT_AP_STOP:
                g_ap_netif_up = false;
                if (!g_sta_netif_up) {
                    g_status.mode = WIFI_MODE_OFF;
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            const ip_event_got_ip_t *got_ip = (const ip_event_got_ip_t *)event_data;
            g_sta_netif_up = true;
            g_status.connected = true;
            g_status.mode = WIFI_MODE_STA;
            (void)wifi_format_ip_address(&got_ip->ip_info, g_status.ip_address, sizeof(g_status.ip_address));
            xEventGroupSetBits(g_wifi_events, WIFI_EVENT_STA_CONNECTED_BIT);
        }
    }
}

static error_code_t wifi_ensure_ready(void) {
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err;

    if (g_wifi_ready) {
        return ERR_OK;
    }

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) {
            return ERR_INTERNAL;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return ERR_INTERNAL;
    }

    if (esp_netif_init() != ESP_OK) {
        return ERR_INTERNAL;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return ERR_INTERNAL;
    }

    if (!g_sta_netif) {
        g_sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (!g_ap_netif) {
        g_ap_netif = esp_netif_create_default_wifi_ap();
    }
    if (!g_sta_netif || !g_ap_netif) {
        return ERR_INTERNAL;
    }

    if (!g_wifi_events) {
        g_wifi_events = xEventGroupCreate();
        if (!g_wifi_events) {
            return ERR_INTERNAL;
        }
    }

    if (esp_wifi_init(&init_cfg) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        return ERR_INTERNAL;
    }

    g_wifi_ready = true;
    return ERR_OK;
}

static error_code_t wifi_apply_hostname(void) {
    if (g_pending_hostname[0] == '\0' || !g_sta_netif) {
        return ERR_OK;
    }

    if (esp_netif_set_hostname(g_sta_netif, g_pending_hostname) != ESP_OK) {
        return ERR_INTERNAL;
    }

    strncpy(g_status.hostname, g_pending_hostname, sizeof(g_status.hostname) - 1U);
    g_status.hostname[sizeof(g_status.hostname) - 1U] = '\0';
    return ERR_OK;
}

static error_code_t wifi_set_mode_and_start(esp_idf_wifi_mode_t mode) {
    if (wifi_ensure_ready() != ERR_OK) {
        return ERR_INTERNAL;
    }

    (void)esp_wifi_stop();
    if (esp_wifi_set_mode(mode) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_start() != ESP_OK) {
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

static void wifi_update_sta_ap_info(void) {
    wifi_ap_record_t ap_info;

    if (g_status.mode != WIFI_MODE_STA || !g_status.connected) {
        return;
    }
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return;
    }

    g_status.rssi = ap_info.rssi;
    strncpy(g_status.ssid, (const char *)ap_info.ssid, sizeof(g_status.ssid) - 1U);
    g_status.ssid[sizeof(g_status.ssid) - 1U] = '\0';
}
#endif

error_code_t wifi_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(&g_status, 0, sizeof(g_status));
    g_status.mode = WIFI_MODE_OFF;
#ifdef ESP_PLATFORM
    memset(g_pending_hostname, 0, sizeof(g_pending_hostname));
    return wifi_ensure_ready();
#else
    return ERR_OK;
#endif
}
error_code_t wifi_set_hostname(const char *hostname) {
    if (!hostname) {
        return ERR_INVALID_ARGS;
    }

#ifdef ESP_PLATFORM
    strncpy(g_pending_hostname, hostname, sizeof(g_pending_hostname) - 1U);
    g_pending_hostname[sizeof(g_pending_hostname) - 1U] = '\0';
    return wifi_apply_hostname();
#else
    strncpy(g_status.hostname, hostname, sizeof(g_status.hostname) - 1U);
    g_status.hostname[sizeof(g_status.hostname) - 1U] = '\0';
    return ERR_OK;
#endif
}
error_code_t wifi_start_ap(const wifi_ap_config_t *cfg, char *out_ip, uint32_t out_ip_len) {
    if (!cfg) {
        return ERR_INVALID_ARGS;
    }

#ifdef ESP_PLATFORM
    wifi_config_t ap_cfg;
    esp_netif_ip_info_t ip_info;

    memset(&ap_cfg, 0, sizeof(ap_cfg));
    strncpy((char *)ap_cfg.ap.ssid, cfg->ssid, sizeof(ap_cfg.ap.ssid) - 1U);
    strncpy((char *)ap_cfg.ap.password, cfg->password, sizeof(ap_cfg.ap.password) - 1U);
    ap_cfg.ap.ssid_len = (uint8_t)strnlen(cfg->ssid, sizeof(cfg->ssid));
    ap_cfg.ap.channel = cfg->channel;
    ap_cfg.ap.max_connection = 4U;
    ap_cfg.ap.authmode = cfg->wpa2_enabled ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    if (wifi_set_mode_and_start(ESP_IDF_WIFI_MODE_APSTA) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_set_config(WIFI_IF_AP, &ap_cfg) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_netif_get_ip_info(g_ap_netif, &ip_info) != ESP_OK) {
        return ERR_INTERNAL;
    }

    g_status.mode = WIFI_MODE_AP;
    g_status.connected = false;
    g_sta_netif_up = false;
    strncpy(g_status.ssid, cfg->ssid, sizeof(g_status.ssid) - 1U);
    g_status.ssid[sizeof(g_status.ssid) - 1U] = '\0';
    if (wifi_format_ip_address(&ip_info, g_status.ip_address, sizeof(g_status.ip_address)) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (out_ip && out_ip_len > 0U) {
        strncpy(out_ip, g_status.ip_address, out_ip_len - 1U);
        out_ip[out_ip_len - 1U] = '\0';
    }
    return ERR_OK;
#else
    g_status.mode = WIFI_MODE_AP;
    g_status.connected = false;
    strncpy(g_status.ssid, cfg->ssid, sizeof(g_status.ssid) - 1U);
    if (out_ip && out_ip_len > 10) {
        strncpy(out_ip, "192.168.4.1", out_ip_len - 1);
        out_ip[out_ip_len - 1] = '\0';
        strncpy(g_status.ip_address, out_ip, sizeof(g_status.ip_address) - 1U);
    }
    return ERR_OK;
#endif
}
error_code_t wifi_stop_ap(void) {
#ifdef ESP_PLATFORM
    if (wifi_ensure_ready() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_stop() != ESP_OK) {
        return ERR_INTERNAL;
    }
    g_ap_netif_up = false;
    if (!g_sta_netif_up) {
        g_status.mode = WIFI_MODE_OFF;
        g_status.ssid[0] = '\0';
        g_status.ip_address[0] = '\0';
    }
    return ERR_OK;
#else
    if (g_status.mode == WIFI_MODE_AP) g_status.mode = WIFI_MODE_OFF;
    return ERR_OK;
#endif
}
error_code_t wifi_connect_sta(const wifi_sta_credentials_t *creds, uint32_t timeout_ms) {
    if (!creds) return ERR_INVALID_ARGS;
#ifdef ESP_PLATFORM
    wifi_config_t sta_cfg;
    EventBits_t bits;

    if (wifi_set_mode_and_start(ESP_IDF_WIFI_MODE_STA) != ERR_OK) {
        return ERR_INTERNAL;
    }

    memset(&sta_cfg, 0, sizeof(sta_cfg));
    strncpy((char *)sta_cfg.sta.ssid, creds->ssid, sizeof(sta_cfg.sta.ssid) - 1U);
    strncpy((char *)sta_cfg.sta.password, creds->password, sizeof(sta_cfg.sta.password) - 1U);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    if (wifi_apply_hostname() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_set_config(WIFI_IF_STA, &sta_cfg) != ESP_OK) {
        return ERR_INTERNAL;
    }

    xEventGroupClearBits(g_wifi_events, WIFI_EVENT_STA_CONNECTED_BIT | WIFI_EVENT_STA_FAILED_BIT);
    wifi_reset_status_runtime();
    g_status.mode = WIFI_MODE_STA;
    strncpy(g_status.ssid, creds->ssid, sizeof(g_status.ssid) - 1U);
    g_status.ssid[sizeof(g_status.ssid) - 1U] = '\0';

    if (esp_wifi_connect() != ESP_OK) {
        return ERR_WIFI_CONNECT_FAILED;
    }

    bits = xEventGroupWaitBits(
        g_wifi_events,
        WIFI_EVENT_STA_CONNECTED_BIT | WIFI_EVENT_STA_FAILED_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if ((bits & WIFI_EVENT_STA_CONNECTED_BIT) != 0U) {
        wifi_update_sta_ap_info();
        return ERR_OK;
    }
    if ((bits & WIFI_EVENT_STA_FAILED_BIT) != 0U) {
        return ERR_WIFI_CONNECT_FAILED;
    }
    return ERR_TIMEOUT;
#else
    (void)timeout_ms;
    g_status.mode = WIFI_MODE_STA;
    g_status.connected = true;
    g_status.rssi = -42;
    strncpy(g_status.ssid, creds->ssid, sizeof(g_status.ssid) - 1);
    strncpy(g_status.ip_address, "192.168.1.50", sizeof(g_status.ip_address) - 1);
    return ERR_OK;
#endif
}
error_code_t wifi_disconnect_sta(void) {
#ifdef ESP_PLATFORM
    if (wifi_ensure_ready() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_disconnect() != ESP_OK) {
        return ERR_INTERNAL;
    }
#endif
    g_status.connected = false;
    g_status.mode = WIFI_MODE_OFF;
    g_status.ip_address[0] = '\0';
    return ERR_OK;
}
error_code_t wifi_scan(wifi_scan_results_t *out_results, uint32_t timeout_ms) {
    if (!out_results) return ERR_INVALID_ARGS;
#ifdef ESP_PLATFORM
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    uint16_t ap_count = WIFI_MAX_SCAN_RESULTS;
    wifi_ap_record_t records[WIFI_MAX_SCAN_RESULTS];
    EventBits_t bits;
    uint16_t i;

    if (wifi_ensure_ready() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (g_ap_netif_up) {
        if (esp_wifi_set_mode(ESP_IDF_WIFI_MODE_APSTA) != ESP_OK) {
            return ERR_INTERNAL;
        }
    } else if (esp_wifi_set_mode(ESP_IDF_WIFI_MODE_STA) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (esp_wifi_start() != ESP_OK) {
        return ERR_INTERNAL;
    }

    xEventGroupClearBits(g_wifi_events, WIFI_EVENT_SCAN_DONE_BIT);
    if (esp_wifi_scan_start(&scan_cfg, false) != ESP_OK) {
        return ERR_INTERNAL;
    }

    bits = xEventGroupWaitBits(
        g_wifi_events,
        WIFI_EVENT_SCAN_DONE_BIT,
        pdTRUE,
        pdTRUE,
        pdMS_TO_TICKS(timeout_ms));
    if ((bits & WIFI_EVENT_SCAN_DONE_BIT) == 0U) {
        return ERR_TIMEOUT;
    }

    memset(out_results, 0, sizeof(*out_results));
    memset(records, 0, sizeof(records));
    if (esp_wifi_scan_get_ap_records(&ap_count, records) != ESP_OK) {
        return ERR_INTERNAL;
    }

    out_results->count = ap_count;
    for (i = 0U; i < ap_count && i < WIFI_MAX_SCAN_RESULTS; ++i) {
        strncpy(out_results->entries[i].ssid, (const char *)records[i].ssid, sizeof(out_results->entries[i].ssid) - 1U);
        out_results->entries[i].ssid[sizeof(out_results->entries[i].ssid) - 1U] = '\0';
        out_results->entries[i].rssi = records[i].rssi;
        switch (records[i].authmode) {
            case WIFI_AUTH_OPEN:
                strncpy(out_results->entries[i].auth_mode, "OPEN", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            case WIFI_AUTH_WPA_PSK:
                strncpy(out_results->entries[i].auth_mode, "WPA", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            case WIFI_AUTH_WPA2_PSK:
                strncpy(out_results->entries[i].auth_mode, "WPA2", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                strncpy(out_results->entries[i].auth_mode, "WPA/WPA2", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                strncpy(out_results->entries[i].auth_mode, "WPA2/WPA3", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            case WIFI_AUTH_WPA3_PSK:
                strncpy(out_results->entries[i].auth_mode, "WPA3", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
            default:
                strncpy(out_results->entries[i].auth_mode, "SECURED", sizeof(out_results->entries[i].auth_mode) - 1U);
                break;
        }
        out_results->entries[i].auth_mode[sizeof(out_results->entries[i].auth_mode) - 1U] = '\0';
    }
    return ERR_OK;
#else
    (void)timeout_ms;
    memset(out_results, 0, sizeof(*out_results));
    out_results->count = 2U;
    strncpy(out_results->entries[0].ssid, "OfficeNet", sizeof(out_results->entries[0].ssid) - 1U);
    out_results->entries[0].rssi = -45;
    strncpy(out_results->entries[0].auth_mode, "WPA2", sizeof(out_results->entries[0].auth_mode) - 1U);
    strncpy(out_results->entries[1].ssid, "GuestWiFi", sizeof(out_results->entries[1].ssid) - 1U);
    out_results->entries[1].rssi = -67;
    strncpy(out_results->entries[1].auth_mode, "OPEN", sizeof(out_results->entries[1].auth_mode) - 1U);
    return ERR_OK;
#endif
}
error_code_t wifi_get_status(platform_wifi_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
#ifdef ESP_PLATFORM
    wifi_update_sta_ap_info();
#endif
    *out_status = g_status;
    return ERR_OK;
}
