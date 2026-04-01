#pragma once

#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::contracts {

enum class ConfigTransportCommand : std::uint8_t {
  kPersist = 1,
  kLoad = 2,
  kClear = 3,
  kGetCapabilities = 4,
};

struct ConfigTransportRequest {
  std::uint32_t protocol_version{0};
  std::uint32_t request_id{0};
  ConfigTransportCommand command{ConfigTransportCommand::kGetCapabilities};
  MappingBundleRef mapping_bundle{};
  ProfileId profile_id{};
  const std::uint8_t* bonding_material{nullptr};
  std::size_t bonding_material_size{0};
  std::uint32_t integrity{0};
};

struct ConfigTransportCapabilities {
  std::uint32_t protocol_version{0};
  bool supports_persist{false};
  bool supports_load{false};
  bool supports_clear{false};
  bool supports_get_capabilities{false};
  bool supports_ble_transport{false};
};

struct ConfigTransportResponse {
  std::uint32_t protocol_version{0};
  std::uint32_t request_id{0};
  ConfigTransportCommand command{ConfigTransportCommand::kGetCapabilities};
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
  MappingBundleRef mapping_bundle{};
  ProfileId profile_id{};
  const std::uint8_t* bonding_material{nullptr};
  std::size_t bonding_material_size{0};
  ConfigTransportCapabilities capabilities{};
};

}  // namespace charm::contracts
