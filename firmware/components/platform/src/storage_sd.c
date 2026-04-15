#include "storage_sd.h"
#include <string.h>
#include "board.h"

static bool g_mounted = false;
static board_pin_map_t g_pin_map;

error_code_t storage_sd_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }

    /* TODO: bring up the shared SPI bus and SD power gating on GPIO16. */
    return ERR_OK;
}
error_code_t storage_sd_mount(void) { g_mounted = true; return ERR_OK; }
error_code_t storage_sd_unmount(void) { g_mounted = false; return ERR_OK; }
bool storage_sd_is_mounted(void) { return g_mounted; }

bool storage_sd_exists(const char *path, uint32_t *out_size_bytes) {
    if (!path) {
        return false;
    }

    if (strcmp(path, "/assets") == 0 || strcmp(path, "/assets/demo.bmp") == 0) {
        if (out_size_bytes) {
            *out_size_bytes = 4096U;
        }
        return true;
    }

    return false;
}

error_code_t storage_sd_list(const char *path, storage_sd_list_result_t *out_result) {
    if (!path || !out_result) return ERR_INVALID_ARGS;
    if (!g_mounted) return ERR_SD_NOT_MOUNTED;
    memset(out_result, 0, sizeof(*out_result));
    strncpy(out_result->path, path, sizeof(out_result->path) - 1U);
    out_result->entry_count = 2U;
    strncpy(out_result->entries[0].name, "demo.bmp", sizeof(out_result->entries[0].name) - 1U);
    out_result->entries[0].size_bytes = 4096U;
    strncpy(out_result->entries[1].name, "layouts", sizeof(out_result->entries[1].name) - 1U);
    out_result->entries[1].is_directory = true;
    return ERR_OK;
}
