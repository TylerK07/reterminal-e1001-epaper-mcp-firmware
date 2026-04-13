#include "services/provisioning_service.h"
#include <string.h>

static bool g_active = false;

error_code_t provisioning_service_init(void) { return ERR_OK; }
error_code_t provisioning_service_start(void) { g_active = true; return ERR_OK; }
error_code_t provisioning_service_stop(void) { g_active = false; return ERR_OK; }
bool provisioning_service_is_active(void) { return g_active; }

error_code_t provisioning_service_get_status(provisioning_status_t *out_status) {
    if (!out_status) return ERR_INVALID_ARGS;
    memset(out_status, 0, sizeof(*out_status));
    out_status->state = g_active ? PROVISIONING_STATE_ACTIVE : PROVISIONING_STATE_UNPROVISIONED;
    out_status->ap_active = g_active;
    return ERR_OK;
}

error_code_t provisioning_service_submit(const provisioning_submission_t *submission) {
    if (!submission) return ERR_INVALID_ARGS;
    // TODO: validate, persist config, trigger transition to CONNECTING
    (void)submission;
    return ERR_OK;
}
