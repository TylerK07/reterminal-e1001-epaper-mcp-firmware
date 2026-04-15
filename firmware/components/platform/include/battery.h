#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "enums.h"
#include "errors.h"

typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
    bool low_battery;
    bool charging;
    power_policy_t power_policy;
} platform_battery_status_t;

error_code_t battery_init(void);
error_code_t battery_get_status(platform_battery_status_t *out_status);
