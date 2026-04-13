#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/errors.h"
#include "services/config_models.h"

error_code_t config_service_init(void);
error_code_t config_service_get(device_config_t *out_cfg);
error_code_t config_service_get_redacted_json(char *buffer, uint32_t buffer_len, uint32_t *out_len);
error_code_t config_service_patch(const config_patch_req_t *patch, bool *out_restart_required, bool *out_reconnect_required);
error_code_t config_service_set_wifi_credentials(const char *ssid, const char *password);
error_code_t config_service_clear_wifi_credentials(void);
error_code_t config_service_set_auth_token(const char *token);
bool config_service_has_auth_token(void);
