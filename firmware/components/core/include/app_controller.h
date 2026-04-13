#pragma once
#include "enums.h"
#include "errors.h"

error_code_t app_controller_init(void);
error_code_t app_controller_run(void);
app_state_t app_controller_get_state(void);
