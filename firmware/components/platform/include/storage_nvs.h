#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

error_code_t storage_nvs_init(void);
error_code_t storage_nvs_write_blob(const char *namespace_name, const char *key, const void *data, uint32_t data_len);
error_code_t storage_nvs_read_blob(const char *namespace_name, const char *key, void *buffer, uint32_t buffer_len, uint32_t *out_len);
error_code_t storage_nvs_erase_key(const char *namespace_name, const char *key);
error_code_t storage_nvs_key_exists(const char *namespace_name, const char *key, bool *out_exists);
