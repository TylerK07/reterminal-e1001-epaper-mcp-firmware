#pragma once
#include "core/errors.h"
#include "services/status_models.h"

error_code_t battery_init(void);
error_code_t battery_get_status(battery_status_t *out_status);
