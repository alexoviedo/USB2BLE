#include <nvs.h>
#include <nvs_flash.h>

int nvs_flash_init(void) {
    return 0;
}

int nvs_flash_erase(void) {
    return 0;
}

int nvs_open(const char* name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    if (out_handle) {
        *out_handle = 1;
    }
    return 0;
}

int nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length) {
    return 0;
}

int nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length) {
    return 0;
}

int nvs_commit(nvs_handle_t handle) {
    return 0;
}

void nvs_close(nvs_handle_t handle) {
}

int nvs_erase_all(nvs_handle_t handle) {
    return 0;
}

int nvs_erase_key(nvs_handle_t handle, const char* key) {
    return 0;
}
