#pragma once
#include <stdint.h>
#include "core/errors.h"
#include "services/status_models.h"

error_code_t status_service_init(void);
error_code_t status_service_get_snapshot(device_status_snapshot_t *out_snapshot);
error_code_t status_service_get_snapshot_json(char *buffer, uint32_t buffer_len, uint32_t *out_len);
