#include "mcp/mcp_dispatch.h"
#include <string.h>

error_code_t mcp_dispatch_init(void) { return ERR_OK; }

error_code_t mcp_dispatch_handle(const mcp_request_t *req, mcp_response_t *out_resp) {
    if (!req || !out_resp) return ERR_INVALID_ARGS;
    memset(out_resp, 0, sizeof(*out_resp));
    out_resp->ok = false;
    out_resp->error_code = ERR_UNSUPPORTED;
    strncpy(out_resp->error_message, "TODO: implement MCP dispatch", sizeof(out_resp->error_message) - 1);
    strncpy(out_resp->mime_type, "application/json", sizeof(out_resp->mime_type) - 1);
    strncpy(out_resp->body, "{\"todo\":\"mcp dispatch\"}", sizeof(out_resp->body) - 1);
    return ERR_OK;
}
