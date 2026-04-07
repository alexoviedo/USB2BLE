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
  bool is_relative{false};
  bool is_array{false};
  bool has_null_state{false};
  bool has_usage_range{false};
  charm::contracts::Usage usage_min{0};
  charm::contracts::Usage usage_max{0};
  std::int32_t logical_min{0};
  std::int32_t logical_max{0};
};

struct DecodePlanInput {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::InterfaceHandle interface_handle{};
  charm::contracts::InterfaceNumber interface_number{0};
  charm::contracts::VendorId vendor_id{0};
  charm::contracts::ProductId product_id{0};
  charm::contracts::HubPath hub_path{};
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

charm::contracts::ElementKeyHash ComputeElementKeyHash(
    const charm::contracts::ElementKey& key);
charm::contracts::ElementKey MakeElementKeyForUsage(const DecodeBinding& binding,
                                                    charm::contracts::Usage usage);

}  // namespace charm::core
