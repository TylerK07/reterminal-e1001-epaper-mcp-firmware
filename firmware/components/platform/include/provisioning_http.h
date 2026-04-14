#pragma once
#include <stdbool.h>
#include "errors.h"

typedef struct {
    char ssid[64];
    char password[128];
    char device_name[64];
    char auth_token[128];
} provisioning_http_submission_t;

typedef error_code_t (*provisioning_http_submit_fn_t)(const provisioning_http_submission_t *submission);

typedef struct {
    provisioning_http_submit_fn_t submit;
} provisioning_http_handlers_t;

error_code_t provisioning_http_init(void);
error_code_t provisioning_http_start(const provisioning_http_handlers_t *handlers);
error_code_t provisioning_http_stop(void);
bool provisioning_http_is_running(void);
