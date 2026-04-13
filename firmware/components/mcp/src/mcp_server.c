#include "mcp/mcp_server.h"

static mcp_server_status_t g_status;

error_code_t mcp_server_init(void) { g_status.port = 0; g_status.running = false; return ERR_OK; }
error_code_t mcp_server_start(uint16_t port) { g_status.port = port; g_status.running = true; return ERR_OK; }
error_code_t mcp_server_stop(void) { g_status.running = false; return ERR_OK; }
error_code_t mcp_server_get_status(mcp_server_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    *out_status = g_status;
    return ERR_OK;
}
