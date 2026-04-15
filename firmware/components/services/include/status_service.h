#pragma once
#include <stdint.h>
#include "errors.h"
#include "status_models.h"

error_code_t status_service_init(void);
error_code_t status_service_get_device_identity(device_identity_t *out_device);
error_code_t status_service_get_battery_status(battery_status_t *out_status);
error_code_t status_service_get_wifi_status(wifi_status_t *out_status);
error_code_t status_service_get_display_status(display_status_t *out_status);
error_code_t status_service_get_environment_status(environment_status_t *out_status);
error_code_t status_service_get_provisioning_status(provisioning_status_t *out_status);
error_code_t status_service_get_snapshot(device_status_snapshot_t *out_snapshot);
error_code_t status_service_get_snapshot_json(char *buffer, uint32_t buffer_len, uint32_t *out_len);
