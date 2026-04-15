#include "mcp_registry.h"
#include <stddef.h>
#include <string.h>

static const mcp_tool_metadata_t g_tools[] = {
    {"get_device_info", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"get_battery_status", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"get_environment_status", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"get_wifi_status", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"scan_wifi", MCP_AUTH_PRIVILEGED, MCP_COST_MEDIUM},
    {"connect_wifi", MCP_AUTH_PRIVILEGED, MCP_COST_MEDIUM},
    {"disconnect_wifi", MCP_AUTH_PRIVILEGED, MCP_COST_LOW},
    {"list_assets", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"get_config", MCP_AUTH_PRIVILEGED, MCP_COST_LOW},
    {"set_config", MCP_AUTH_PRIVILEGED, MCP_COST_MEDIUM},
    {"render_text", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"clear_region", MCP_AUTH_STANDARD, MCP_COST_LOW},
    {"render_bitmap", MCP_AUTH_STANDARD, MCP_COST_MEDIUM},
    {"render_layout", MCP_AUTH_STANDARD, MCP_COST_MEDIUM},
    {"refresh_display", MCP_AUTH_STANDARD, MCP_COST_HIGH},
    {"sleep_now", MCP_AUTH_PRIVILEGED, MCP_COST_HIGH},
    {"restart_device", MCP_AUTH_PRIVILEGED, MCP_COST_HIGH},
    {"reset_network", MCP_AUTH_PRIVILEGED, MCP_COST_HIGH},
};

static const mcp_resource_metadata_t g_resources[] = {
    {"device://status", MCP_AUTH_STANDARD},
    {"device://power/battery", MCP_AUTH_STANDARD},
    {"device://environment", MCP_AUTH_STANDARD},
    {"device://network/wifi", MCP_AUTH_STANDARD},
    {"device://config", MCP_AUTH_PRIVILEGED},
    {"device://display/capabilities", MCP_AUTH_STANDARD},
    {"device://storage/assets", MCP_AUTH_STANDARD},
    {"device://render/last-job", MCP_AUTH_STANDARD},
};

error_code_t mcp_registry_init(void) {
    return ERR_OK;
}

const mcp_tool_metadata_t *mcp_registry_find_tool(const char *name) {
    uint32_t i;
    if (!name) {
        return NULL;
    }

    for (i = 0; i < (uint32_t)(sizeof(g_tools) / sizeof(g_tools[0])); ++i) {
        if (strcmp(name, g_tools[i].name) == 0) {
            return &g_tools[i];
        }
    }

    return NULL;
}

const mcp_resource_metadata_t *mcp_registry_find_resource(const char *uri) {
    uint32_t i;
    if (!uri) {
        return NULL;
    }

    for (i = 0; i < (uint32_t)(sizeof(g_resources) / sizeof(g_resources[0])); ++i) {
        if (strcmp(uri, g_resources[i].uri) == 0) {
            return &g_resources[i];
        }
    }

    return NULL;
}

uint32_t mcp_registry_get_tool_count(void) {
    return (uint32_t)(sizeof(g_tools) / sizeof(g_tools[0]));
}

uint32_t mcp_registry_get_resource_count(void) {
    return (uint32_t)(sizeof(g_resources) / sizeof(g_resources[0]));
}
