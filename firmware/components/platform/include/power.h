#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/enums.h"
#include "core/errors.h"

typedef struct {
    bool timer_wake_enabled;
    uint32_t wake_after_sec;
    bool button_wake_enabled;
} wake_config_t;

error_code_t power_init(void);
wake_reason_t power_get_wake_reason(void);
error_code_t power_enter_deep_sleep(const wake_config_t *cfg);
error_code_t power_get_policy(power_policy_t *out_policy);
bool power_operation_allowed(power_policy_t policy, const char *operation_name);
