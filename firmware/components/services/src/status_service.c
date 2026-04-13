#include "services/status_service.h"
#include <string.h>

error_code_t status_service_init(void) { return ERR_OK; }

error_code_t status_service_get_snapshot(device_status_snapshot_t *out_snapshot) {
    if (!out_snapshot) return ERR_INVALID_ARGS;
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    return ERR_OK;
}

error_code_t status_service_get_snapshot_json(char *buffer, uint32_t buffer_len, uint32_t *out_len) {
    const char *json = "{\"todo\":\"status snapshot json\"}";
    size_t len = strlen(json);
    if (!buffer || buffer_len <= len) return ERR_INVALID_ARGS;
    memcpy(buffer, json, len + 1);
    if (out_len) *out_len = (uint32_t)len;
    return ERR_OK;
}
