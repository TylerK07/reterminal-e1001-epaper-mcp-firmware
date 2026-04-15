#include "storage_nvs.h"
#include <stddef.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "nvs.h"
#include "nvs_flash.h"
#endif

#ifdef ESP_PLATFORM
static bool g_nvs_ready = false;

static error_code_t storage_nvs_validate_args(const char *namespace_name, const char *key) {
    if (!namespace_name || namespace_name[0] == '\0' || !key || key[0] == '\0') {
        return ERR_INVALID_ARGS;
    }
    return ERR_OK;
}

static error_code_t storage_nvs_validate_namespace(const char *namespace_name) {
    if (!namespace_name || namespace_name[0] == '\0') {
        return ERR_INVALID_ARGS;
    }
    return ERR_OK;
}

static error_code_t storage_nvs_open(const char *namespace_name, nvs_open_mode_t mode, nvs_handle_t *out_handle) {
    esp_err_t nvs_err;

    if (storage_nvs_validate_namespace(namespace_name) != ERR_OK || !out_handle) {
        return ERR_INVALID_ARGS;
    }
    if (storage_nvs_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    nvs_err = nvs_open(namespace_name, mode, out_handle);
    if (nvs_err == ESP_ERR_NVS_NOT_FOUND) {
        return ERR_NOT_FOUND;
    }
    if (nvs_err != ESP_OK) {
        return ERR_INTERNAL;
    }
    return ERR_OK;
}
#endif

error_code_t storage_nvs_init(void) {
#ifdef ESP_PLATFORM
    esp_err_t err;

    if (g_nvs_ready) {
        return ERR_OK;
    }

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) {
            return ERR_INTERNAL;
        }
        err = nvs_flash_init();
    }

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_INITIALIZED) {
        if (err == ESP_ERR_INVALID_STATE) {
            g_nvs_ready = true;
            return ERR_OK;
        }
        return ERR_INTERNAL;
    }

    g_nvs_ready = true;
    return ERR_OK;
#else
    return ERR_OK;
#endif
}

error_code_t storage_nvs_write_blob(const char *namespace_name, const char *key, const void *data, uint32_t data_len) {
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    error_code_t err;

    if (!data || data_len == 0U) {
        return ERR_INVALID_ARGS;
    }

    err = storage_nvs_validate_args(namespace_name, key);
    if (err != ERR_OK) {
        return err;
    }

    err = storage_nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ERR_OK) {
        return err;
    }

    if (nvs_set_blob(handle, key, data, (size_t)data_len) != ESP_OK || nvs_commit(handle) != ESP_OK) {
        nvs_close(handle);
        return ERR_INTERNAL;
    }

    nvs_close(handle);
    return ERR_OK;
#else
    (void)namespace_name;
    (void)key;
    (void)data;
    (void)data_len;
    return ERR_OK;
#endif
}

error_code_t storage_nvs_read_blob(const char *namespace_name, const char *key, void *buffer, uint32_t buffer_len, uint32_t *out_len) {
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    size_t required_size = 0U;
    error_code_t err;
    esp_err_t nvs_err;

    if (!buffer || buffer_len == 0U) {
        return ERR_INVALID_ARGS;
    }

    err = storage_nvs_validate_args(namespace_name, key);
    if (err != ERR_OK) {
        return err;
    }

    err = storage_nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ERR_OK) {
        return (err == ERR_NOT_FOUND) ? ERR_NOT_FOUND : err;
    }

    nvs_err = nvs_get_blob(handle, key, NULL, &required_size);
    if (nvs_err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ERR_NOT_FOUND;
    }
    if (nvs_err != ESP_OK) {
        nvs_close(handle);
        return ERR_INTERNAL;
    }
    if (required_size > (size_t)buffer_len) {
        nvs_close(handle);
        return ERR_INVALID_ARGS;
    }
    if (nvs_get_blob(handle, key, buffer, &required_size) != ESP_OK) {
        nvs_close(handle);
        return ERR_INTERNAL;
    }

    nvs_close(handle);
    if (out_len) {
        *out_len = (uint32_t)required_size;
    }
    return ERR_OK;
#else
    (void)namespace_name;
    (void)key;
    (void)buffer;
    (void)buffer_len;
    if (out_len) {
        *out_len = 0U;
    }
    return ERR_NOT_FOUND;
#endif
}

error_code_t storage_nvs_erase_key(const char *namespace_name, const char *key) {
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    error_code_t err;
    esp_err_t nvs_err;

    err = storage_nvs_validate_args(namespace_name, key);
    if (err != ERR_OK) {
        return err;
    }

    err = storage_nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ERR_OK) {
        return (err == ERR_NOT_FOUND) ? ERR_NOT_FOUND : err;
    }

    nvs_err = nvs_erase_key(handle, key);
    if (nvs_err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ERR_NOT_FOUND;
    }
    if (nvs_err != ESP_OK || nvs_commit(handle) != ESP_OK) {
        nvs_close(handle);
        return ERR_INTERNAL;
    }

    nvs_close(handle);
    return ERR_OK;
#else
    (void)namespace_name;
    (void)key;
    return ERR_OK;
#endif
}

error_code_t storage_nvs_key_exists(const char *namespace_name, const char *key, bool *out_exists) {
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    size_t required_size = 0U;
    error_code_t err;
    esp_err_t nvs_err;

    if (!out_exists) {
        return ERR_INVALID_ARGS;
    }

    *out_exists = false;
    err = storage_nvs_validate_args(namespace_name, key);
    if (err != ERR_OK) {
        return err;
    }

    err = storage_nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ERR_OK) {
        return (err == ERR_NOT_FOUND) ? ERR_OK : err;
    }

    nvs_err = nvs_get_blob(handle, key, NULL, &required_size);
    nvs_close(handle);
    if (nvs_err == ESP_ERR_NVS_NOT_FOUND) {
        return ERR_OK;
    }
    if (nvs_err != ESP_OK) {
        return ERR_INTERNAL;
    }

    *out_exists = true;
    return ERR_OK;
#else
    (void)namespace_name;
    (void)key;
    if (!out_exists) {
        return ERR_INVALID_ARGS;
    }
    *out_exists = false;
    return ERR_OK;
#endif
}
