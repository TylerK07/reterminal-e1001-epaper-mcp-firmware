#pragma once
#include <stdint.h>
#include "asset_models.h"
#include "errors.h"

error_code_t asset_service_init(void);
error_code_t asset_service_list(const char *path, asset_list_result_t *out_result);
error_code_t asset_service_exists(const char *path, asset_info_t *out_info);
error_code_t asset_service_get_list_json(const char *path, char *buffer, uint32_t buffer_len, uint32_t *out_len);
