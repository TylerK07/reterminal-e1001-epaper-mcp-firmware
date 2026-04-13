#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/errors.h"
#include "services/asset_models.h"

error_code_t storage_sd_init(void);
error_code_t storage_sd_mount(void);
error_code_t storage_sd_unmount(void);
bool storage_sd_is_mounted(void);
error_code_t storage_sd_list(const char *path, asset_list_result_t *out_result);
