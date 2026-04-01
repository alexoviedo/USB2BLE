#pragma once

#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::contracts {

enum class ModeState : std::uint8_t;
enum class RecoveryState : std::uint8_t;

struct StartRequest {};

struct StartResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct StopRequest {};

struct StopResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct ActivateMappingBundleRequest {
  MappingBundleRef mapping_bundle{};
};

struct ActivateMappingBundleResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct SelectProfileRequest {
  ProfileId profile_id{};
};

struct SelectProfileResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct PersistConfigRequest {
  MappingBundleRef mapping_bundle{};
  ProfileId profile_id{};
  const std::uint8_t* bonding_material{nullptr};
  std::size_t bonding_material_size{0};
};

struct PersistConfigResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct LoadConfigRequest {};

struct LoadConfigResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
  MappingBundleRef mapping_bundle{};
  ProfileId profile_id{};
  const std::uint8_t* bonding_material{nullptr};
  std::size_t bonding_material_size{0};
};

struct ModeTransitionRequest {
  ModeState target_mode;
};

struct ModeTransitionResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

struct RecoveryRequest {
  RecoveryState target_state;
};

struct RecoveryResult {
  ContractStatus status{ContractStatus::kUnspecified};
  FaultCode fault_code{};
};

}  // namespace charm::contracts
