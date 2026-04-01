#include "charm/platform/config_store_nvs.hpp"
#include <nvs.h>
#include <nvs_flash.h>
#include <cstring>

namespace charm::platform {

static constexpr const char* kNvsNamespace = "charm_cfg";
static constexpr const char* kBundleKey = "map_bundle";
static constexpr const char* kProfileKey = "prof_id";
static constexpr const char* kBondKey = "bond_mat";

ConfigStoreNvs::~ConfigStoreNvs() {
  if (cached_config_.bonding_material != nullptr) {
    delete[] cached_config_.bonding_material;
  }
}

charm::contracts::LoadConfigResult ConfigStoreNvs::LoadConfig(const charm::contracts::LoadConfigRequest& request) {
  charm::contracts::LoadConfigResult result{};
  nvs_handle_t handle;

  if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != 0) {
    result.status = charm::contracts::ContractStatus::kFailed;
    return result;
  }

  charm::contracts::MappingBundleRef temp_bundle{};
  charm::contracts::ProfileId temp_profile_id{};

  size_t length = sizeof(temp_bundle);
  if (nvs_get_blob(handle, kBundleKey, &temp_bundle, &length) != 0) {
    result.status = charm::contracts::ContractStatus::kFailed;
    nvs_close(handle);
    return result;
  }

  length = sizeof(temp_profile_id);
  if (nvs_get_blob(handle, kProfileKey, &temp_profile_id, &length) != 0) {
    result.status = charm::contracts::ContractStatus::kFailed;
    nvs_close(handle);
    return result;
  }

  // Load bonding material length
  size_t bond_length = 0;
  std::uint8_t* temp_bonding_material = nullptr;

  if (nvs_get_blob(handle, kBondKey, nullptr, &bond_length) == 0 && bond_length > 0) {
    temp_bonding_material = new std::uint8_t[bond_length];
    if (nvs_get_blob(handle, kBondKey, temp_bonding_material, &bond_length) != 0) {
      delete[] temp_bonding_material;
      result.status = charm::contracts::ContractStatus::kFailed;
      nvs_close(handle);
      return result;
    }
  }

  nvs_close(handle);

  // Successfully loaded everything, update cache
  cached_config_.mapping_bundle = temp_bundle;
  cached_config_.profile_id = temp_profile_id;

  if (cached_config_.bonding_material != nullptr) {
    delete[] cached_config_.bonding_material;
  }

  cached_config_.bonding_material = temp_bonding_material;
  cached_config_.bonding_material_size = bond_length;

  result.status = charm::contracts::ContractStatus::kOk;
  result.mapping_bundle = cached_config_.mapping_bundle;
  result.profile_id = cached_config_.profile_id;
  result.bonding_material = cached_config_.bonding_material;
  result.bonding_material_size = cached_config_.bonding_material_size;
  return result;
}

charm::contracts::PersistConfigResult ConfigStoreNvs::PersistConfig(const charm::contracts::PersistConfigRequest& request) {
  charm::contracts::PersistConfigResult result{};
  nvs_handle_t handle;

  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != 0) {
    result.status = charm::contracts::ContractStatus::kFailed;
    return result;
  }

  std::uint8_t* new_bonding_material = nullptr;

  if (request.bonding_material != nullptr && request.bonding_material_size > 0) {
    if (nvs_set_blob(handle, kBondKey, request.bonding_material, request.bonding_material_size) != 0) {
      result.status = charm::contracts::ContractStatus::kFailed;
      nvs_close(handle);
      return result;
    }
    new_bonding_material = new std::uint8_t[request.bonding_material_size];
    std::memcpy(new_bonding_material, request.bonding_material, request.bonding_material_size);
  } else {
    nvs_erase_key(handle, kBondKey); // ignoring result because it might legitimately not exist
  }

  if (nvs_set_blob(handle, kBundleKey, &request.mapping_bundle, sizeof(request.mapping_bundle)) != 0 ||
      nvs_set_blob(handle, kProfileKey, &request.profile_id, sizeof(request.profile_id)) != 0 ||
      nvs_commit(handle) != 0) {
    result.status = charm::contracts::ContractStatus::kFailed;
    if (new_bonding_material != nullptr) {
      delete[] new_bonding_material;
    }
  } else {
    // Commit was successful, update the cache
    if (cached_config_.bonding_material != nullptr) {
      delete[] cached_config_.bonding_material;
    }

    cached_config_.mapping_bundle = request.mapping_bundle;
    cached_config_.profile_id = request.profile_id;
    cached_config_.bonding_material = new_bonding_material;
    cached_config_.bonding_material_size = new_bonding_material ? request.bonding_material_size : 0;

    result.status = charm::contracts::ContractStatus::kOk;
  }

  nvs_close(handle);

  return result;
}

charm::ports::ClearConfigResult ConfigStoreNvs::ClearConfig(const charm::ports::ClearConfigRequest& request) {
  charm::ports::ClearConfigResult result{};
  nvs_handle_t handle;

  result.status = charm::contracts::ContractStatus::kFailed;

  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == 0) {
    if (nvs_erase_all(handle) == 0 && nvs_commit(handle) == 0) {
      result.status = charm::contracts::ContractStatus::kOk;
      if (cached_config_.bonding_material != nullptr) {
        delete[] cached_config_.bonding_material;
      }
      cached_config_ = charm::ports::PersistedConfigRecord{};
    }
    nvs_close(handle);
  }

  return result;
}

charm::ports::PersistedConfigRecord ConfigStoreNvs::PeekPersistedConfig() const {
  return cached_config_;
}

}  // namespace charm::platform
