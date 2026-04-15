#pragma once
#include "errors.h"
#include "mcp_models.h"

error_code_t mcp_dispatch_init(void);
error_code_t mcp_dispatch_handle(const mcp_request_t *req, mcp_response_t *out_resp);
