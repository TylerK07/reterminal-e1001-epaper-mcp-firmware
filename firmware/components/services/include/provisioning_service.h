#pragma once
#include <stdbool.h>
#include "core/errors.h"
#include "services/status_models.h"

typedef struct {
    char ssid[64];
    char password[128];
    char device_name[64];
    char auth_token[128];
} provisioning_submission_t;

error_code_t provisioning_service_init(void);
error_code_t provisioning_service_start(void);
error_code_t provisioning_service_stop(void);
bool provisioning_service_is_active(void);
error_code_t provisioning_service_get_status(provisioning_status_t *out_status);
error_code_t provisioning_service_submit(const provisioning_submission_t *submission);
