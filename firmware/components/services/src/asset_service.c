#include "asset_service.h"
#include <stdio.h>
#include <string.h>
#include "storage_sd.h"

error_code_t asset_service_init(void) {
    if (!storage_sd_is_mounted()) {
        return storage_sd_mount();
    }

    return ERR_OK;
}

error_code_t asset_service_list(const char *path, asset_list_result_t *out_result) {
    storage_sd_list_result_t storage_result;
    uint16_t i;
    error_code_t err;

    if (!path || !out_result) {
        return ERR_INVALID_ARGS;
    }

    err = storage_sd_list(path, &storage_result);
    if (err != ERR_OK) {
        return err;
    }

    memset(out_result, 0, sizeof(*out_result));
    strncpy(out_result->path, storage_result.path, sizeof(out_result->path) - 1U);
    out_result->entry_count = storage_result.entry_count;

    for (i = 0; i < storage_result.entry_count && i < MAX_ASSET_ENTRIES; ++i) {
        strncpy(out_result->entries[i].name, storage_result.entries[i].name, sizeof(out_result->entries[i].name) - 1U);
        out_result->entries[i].type = storage_result.entries[i].is_directory ? ASSET_TYPE_DIRECTORY : ASSET_TYPE_FILE;
        out_result->entries[i].size_bytes = storage_result.entries[i].size_bytes;
    }

    return ERR_OK;
}

error_code_t asset_service_exists(const char *path, asset_info_t *out_info) {
    if (!path || !out_info) {
        return ERR_INVALID_ARGS;
    }

    memset(out_info, 0, sizeof(*out_info));
    out_info->exists = storage_sd_exists(path, &out_info->size_bytes);
    return ERR_OK;
}

error_code_t asset_service_get_list_json(const char *path, char *buffer, uint32_t buffer_len, uint32_t *out_len) {
    asset_list_result_t result;
    int written;
    error_code_t err = asset_service_list(path, &result);
    if (err != ERR_OK) {
        return err;
    }

    written = snprintf(
        buffer,
        buffer_len,
        "{\"path\":\"%s\",\"entry_count\":%u}",
        result.path,
        (unsigned int)result.entry_count);
    if (!buffer || written < 0 || (uint32_t)written >= buffer_len) {
        return ERR_INVALID_ARGS;
    }

    if (out_len) {
        *out_len = (uint32_t)written;
    }

    return ERR_OK;
}
