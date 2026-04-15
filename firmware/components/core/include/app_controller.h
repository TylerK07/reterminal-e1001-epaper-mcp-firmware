#pragma once
#include <stdbool.h>
#include "enums.h"
#include "errors.h"

error_code_t app_controller_init(void);
error_code_t app_controller_run(void);
error_code_t app_controller_tick(void);
app_state_t app_controller_get_state(void);
bool app_controller_is_mcp_enabled(void);
