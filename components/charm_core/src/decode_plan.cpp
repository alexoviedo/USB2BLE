#include "charm/core/decode_plan.hpp"

#include <algorithm>

namespace charm::core {

namespace {

// FNV-1a 32-bit hash function as requested by constraints
constexpr std::uint32_t kFnvPrime32 = 0x01000193;
constexpr std::uint32_t kFnvOffsetBasis32 = 0x811C9DC5;

constexpr charm::contracts::UsagePage kUsagePageGenericDesktop = 0x01;
constexpr charm::contracts::UsagePage kUsagePageSimulationControls = 0x02;
constexpr charm::contracts::UsagePage kUsagePageButton = 0x09;
constexpr charm::contracts::UsagePage kUsagePageKeyboard = 0x07;

constexpr charm::contracts::Usage kUsageGenericX = 0x30;
constexpr charm::contracts::Usage kUsageGenericWheel = 0x38;
constexpr charm::contracts::Usage kUsageGenericHat = 0x39;

constexpr charm::contracts::Usage kUsageSimulationAileron = 0xB0;
constexpr charm::contracts::Usage kUsageSimulationRudder = 0xBA;
constexpr charm::contracts::Usage kUsageSimulationThrottle = 0xBB;
constexpr charm::contracts::Usage kUsageSimulationAccelerator = 0xC4;
constexpr charm::contracts::Usage kUsageSimulationBrake = 0xC5;
constexpr charm::contracts::Usage kUsageSimulationClutch = 0xC6;

bool IsGenericAxisUsage(charm::contracts::Usage usage) {
  return usage >= kUsageGenericX && usage <= kUsageGenericWheel;
}

bool IsSimulationAxisUsage(charm::contracts::Usage usage) {
  switch (usage) {
    case kUsageSimulationAileron:
    case kUsageSimulationRudder:
      return true;
    default:
      return false;
  }
}

bool IsSimulationTriggerUsage(charm::contracts::Usage usage) {
  switch (usage) {
    case kUsageSimulationThrottle:
    case kUsageSimulationAccelerator:
    case kUsageSimulationBrake:
    case kUsageSimulationClutch:
      return true;
    default:
      return false;
  }
}

charm::contracts::InputElementType DetermineElementType(
    charm::contracts::UsagePage usage_page, charm::contracts::Usage usage) {
  if (usage_page == kUsagePageGenericDesktop) {
    if (IsGenericAxisUsage(usage)) {
      return charm::contracts::InputElementType::kAxis;
    }
    if (usage == kUsageGenericHat) {
      return charm::contracts::InputElementType::kHat;
    }
  } else if (usage_page == kUsagePageSimulationControls) {
    if (IsSimulationAxisUsage(usage)) {
      return charm::contracts::InputElementType::kAxis;
    }
    if (IsSimulationTriggerUsage(usage)) {
      return charm::contracts::InputElementType::kTrigger;
    }
  } else if (usage_page == kUsagePageButton || usage_page == kUsagePageKeyboard) {
    return charm::contracts::InputElementType::kButton;
  }

  return charm::contracts::InputElementType::kScalar;
}

}  // namespace

charm::contracts::ElementKeyHash ComputeElementKeyHash(
    const charm::contracts::ElementKey& key) {
  std::uint32_t hash = kFnvOffsetBasis32;
  const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&key);

  for (std::size_t i = 0; i < sizeof(charm::contracts::ElementKey); ++i) {
    hash ^= ptr[i];
    hash *= kFnvPrime32;
  }

  return charm::contracts::ElementKeyHash{hash};
}

charm::contracts::ElementKey MakeElementKeyForUsage(const DecodeBinding& binding,
                                                    charm::contracts::Usage usage) {
  auto key = binding.element_key;
  key.usage = usage;
  if (binding.is_array) {
    key.logical_index = 0;
  }
  return key;
}

BuildDecodePlanResult DefaultDecodePlanBuilder::BuildDecodePlan(const BuildDecodePlanRequest& request) const {
  BuildDecodePlanResult result;

  const auto& semantic_model = request.input.semantic_model;
  DecodePlan plan;

  if (semantic_model.field_count > kMaxDecodeBindingsPerInterface) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kCapacityExceeded,
        .reason = 5};
    return result;
  }

  for (std::size_t i = 0; i < semantic_model.field_count; ++i) {
    if (plan.binding_count >= kMaxDecodeBindingsPerInterface) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kCapacityExceeded,
          .reason = 5}; // Bindings capacity exceeded
      return result;
    }

    const auto& field = semantic_model.fields[i];
    if (field.bit_size == 0) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kInvalidRequest,
          .reason = 8};
      return result;
    }
    DecodeBinding& binding = plan.bindings[plan.binding_count++];

    binding.element_key.vendor_id = request.input.vendor_id;
    binding.element_key.product_id = request.input.product_id;
    binding.element_key.hub_path = request.input.hub_path;
    binding.element_key.interface_number = request.input.interface_number;
    binding.element_key.report_id = field.report_id;
    binding.element_key.usage_page = field.usage_page;
    binding.element_key.usage = field.usage;
    binding.element_key.collection_index = field.collection_index;
    binding.element_key.logical_index = field.logical_index;
    binding.usage_min = field.usage_min;
    binding.usage_max = field.usage_max;
    binding.has_usage_range = field.has_usage_range;
    binding.is_relative = field.is_relative;
    binding.is_array = field.is_array;
    binding.has_null_state = field.has_null_state;
    binding.logical_min = field.logical_min;
    binding.logical_max = field.logical_max;
    binding.element_type = DetermineElementType(field.usage_page, field.usage);
    binding.element_key_hash = ComputeElementKeyHash(binding.element_key);
    binding.report_id = field.report_id;
    binding.bit_offset = field.bit_offset;
    binding.bit_size = field.bit_size;
    binding.is_signed = field.is_signed;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  result.decode_plan = plan;
  return result;
}

}  // namespace charm::core
