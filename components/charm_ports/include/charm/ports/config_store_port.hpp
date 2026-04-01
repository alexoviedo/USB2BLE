#pragma once

#include <cstddef>
#include <cstdint>

#include "charm/contracts/requests.hpp"

namespace charm::ports {

struct ConfigVersion {
  std::uint32_t value{0};
};

struct IntegrityMetadata {
  std::uint32_t value{0};
};

struct PersistedConfigRecord {
  charm::contracts::MappingBundleRef mapping_bundle{};
  charm::contracts::ProfileId profile_id{};
  ConfigVersion config_version{};
  IntegrityMetadata integrity{};
  const std::uint8_t* bonding_material{nullptr};
  std::size_t bonding_material_size{0};
};

struct ClearConfigRequest {};

struct ClearConfigResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

class ConfigStorePort {
 public:
  virtual ~ConfigStorePort() = default;

  virtual charm::contracts::LoadConfigResult LoadConfig(const charm::contracts::LoadConfigRequest& request) = 0;
  virtual charm::contracts::PersistConfigResult PersistConfig(const charm::contracts::PersistConfigRequest& request) = 0;
  virtual ClearConfigResult ClearConfig(const ClearConfigRequest& request) = 0;
  virtual PersistedConfigRecord PeekPersistedConfig() const = 0;
};

}  // namespace charm::ports
