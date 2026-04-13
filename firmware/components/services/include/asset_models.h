#pragma once
#include <stdint.h>

typedef enum {
    ASSET_TYPE_FILE = 0,
    ASSET_TYPE_DIRECTORY
} asset_type_t;

typedef struct {
    char name[128];
    asset_type_t type;
    uint32_t size_bytes;
} asset_entry_t;

#define MAX_ASSET_ENTRIES 200

typedef struct {
    char path[256];
    uint16_t entry_count;
    asset_entry_t entries[MAX_ASSET_ENTRIES];
} asset_list_result_t;
