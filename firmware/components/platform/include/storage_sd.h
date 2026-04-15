#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

#define STORAGE_SD_MAX_ENTRIES 200

typedef struct {
    char name[128];
    bool is_directory;
    uint32_t size_bytes;
} storage_sd_entry_t;

typedef struct {
    char path[256];
    uint16_t entry_count;
    storage_sd_entry_t entries[STORAGE_SD_MAX_ENTRIES];
} storage_sd_list_result_t;

error_code_t storage_sd_init(void);
error_code_t storage_sd_mount(void);
error_code_t storage_sd_unmount(void);
bool storage_sd_is_mounted(void);
bool storage_sd_exists(const char *path, uint32_t *out_size_bytes);
error_code_t storage_sd_list(const char *path, storage_sd_list_result_t *out_result);
