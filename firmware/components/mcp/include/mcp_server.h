#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

typedef struct {
    uint16_t port;
    bool running;
} mcp_server_status_t;

error_code_t mcp_server_init(void);
error_code_t mcp_server_start(uint16_t port);
error_code_t mcp_server_stop(void);
error_code_t mcp_server_get_status(mcp_server_status_t *out_status);
