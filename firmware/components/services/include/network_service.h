#pragma once
#include <stdint.h>
#include "errors.h"
#include "status_models.h"
#include "wifi.h"

error_code_t network_service_init(void);
error_code_t network_service_connect_from_config(void);
error_code_t network_service_disconnect(void);
error_code_t network_service_scan(wifi_scan_results_t *out_results, uint32_t timeout_ms);
error_code_t network_service_get_status(wifi_status_t *out_status);
