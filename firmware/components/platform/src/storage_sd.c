#include "platform/storage_sd.h"
#include <string.h>

static bool g_mounted = false;

error_code_t storage_sd_init(void) { return ERR_OK; }
error_code_t storage_sd_mount(void) { g_mounted = true; return ERR_OK; }
error_code_t storage_sd_unmount(void) { g_mounted = false; return ERR_OK; }
bool storage_sd_is_mounted(void) { return g_mounted; }

error_code_t storage_sd_list(const char *path, asset_list_result_t *out_result) {
    if (!path || !out_result) return ERR_INVALID_ARGS;
    if (!g_mounted) return ERR_SD_NOT_MOUNTED;
    memset(out_result, 0, sizeof(*out_result));
    return ERR_OK;
}
