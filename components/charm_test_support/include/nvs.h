#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY,
    NVS_READWRITE
} nvs_open_mode_t;

int nvs_open(const char* name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
int nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
int nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
int nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
int nvs_erase_all(nvs_handle_t handle);
int nvs_erase_key(nvs_handle_t handle, const char* key);

#ifdef __cplusplus
}
#endif
