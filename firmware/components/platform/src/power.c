#include "power.h"

error_code_t power_init(void) { return ERR_OK; }
wake_reason_t power_get_wake_reason(void) { return WAKE_REASON_COLD_BOOT; }
error_code_t power_enter_deep_sleep(const wake_config_t *cfg) { (void)cfg; return ERR_OK; }
error_code_t power_get_policy(power_policy_t *out_policy) {
    if (!out_policy) return ERR_INVALID_ARGS;
    *out_policy = POWER_POLICY_NORMAL;
    return ERR_OK;
}
bool power_operation_allowed(power_policy_t policy, const char *operation_name) {
    (void)operation_name;
    return policy != POWER_POLICY_CRITICAL;
}
