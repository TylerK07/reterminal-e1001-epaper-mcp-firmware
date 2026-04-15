#include "battery.h"
#include <string.h>
#include "board.h"

static board_pin_map_t g_pin_map;

error_code_t battery_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }

    /* TODO: sample battery voltage via GPIO1 with enable control on GPIO21. */
    return ERR_OK;
}

error_code_t battery_get_status(platform_battery_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    memset(out_status, 0, sizeof(*out_status));
    out_status->voltage_mv = 4000;
    out_status->percent = 75;
    out_status->low_battery = false;
    out_status->charging = false;
    out_status->power_policy = POWER_POLICY_NORMAL;
    return ERR_OK;
}
