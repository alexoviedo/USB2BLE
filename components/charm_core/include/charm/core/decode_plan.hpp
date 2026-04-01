#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/events.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/report_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/core/hid_semantic_model.hpp"

namespace charm::core {

inline constexpr std::size_t kMaxDecodeBindingsPerInterface = 256;

struct DecodeBinding {
  charm::contracts::ElementKey element_key{};
  charm::contracts::ElementKeyHash element_key_hash{};
  charm::contracts::InputElementType element_type{charm::contracts::InputElementType::kUnknown};
  charm::contracts::ReportId report_id{0};
  std::uint16_t bit_offset{0};
  std::uint16_t bit_size{0};
  bool is_signed{false};
};

struct DecodePlanInput {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::InterfaceHandle interface_handle{};
  charm::contracts::InterfaceNumber interface_number{0};
  SemanticDescriptorModel semantic_model{};
};

struct DecodePlan {
  std::array<DecodeBinding, kMaxDecodeBindingsPerInterface> bindings{};
  std::size_t binding_count{0};
};

struct BuildDecodePlanRequest {
  DecodePlanInput input{};
};

struct BuildDecodePlanResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  DecodePlan decode_plan{};
};

class DecodePlanBuilder {
 public:
  virtual ~DecodePlanBuilder() = default;

  virtual BuildDecodePlanResult BuildDecodePlan(const BuildDecodePlanRequest& request) const = 0;
};

class DefaultDecodePlanBuilder final : public DecodePlanBuilder {
 public:
  BuildDecodePlanResult BuildDecodePlan(const BuildDecodePlanRequest& request) const override;
};

}  // namespace charm::core
