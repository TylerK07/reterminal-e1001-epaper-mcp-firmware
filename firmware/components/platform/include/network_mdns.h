#pragma once
#include "errors.h"

error_code_t network_mdns_init(void);
error_code_t network_mdns_start(const char *hostname, const char *instance_name, uint16_t port);
error_code_t network_mdns_stop(void);
