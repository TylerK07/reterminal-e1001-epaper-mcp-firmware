#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "enums.h"
#include "errors.h"

typedef struct {
    mcp_request_method_t request_method;
    char method_name[64];
    char target[128];
    const char *args_json;
    const char *token;
} mcp_request_t;

typedef struct {
    bool ok;
    error_code_t error_code;
    char error_message[128];
    bool retryable;
    char mime_type[64];
    char body[4096];
} mcp_response_t;
