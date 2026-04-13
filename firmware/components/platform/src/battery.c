#include "platform/battery.h"
#include <string.h>

error_code_t battery_init(void) { return ERR_OK; }

error_code_t battery_get_status(battery_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    memset(out_status, 0, sizeof(*out_status));
    out_status->voltage_mv = 4000;
    out_status->percent = 75;
    out_status->power_policy = POWER_POLICY_NORMAL;
    return ERR_OK;
}
