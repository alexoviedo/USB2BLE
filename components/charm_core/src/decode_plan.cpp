#include "charm/core/decode_plan.hpp"

namespace charm::core {

namespace {

// FNV-1a 32-bit hash function as requested by constraints
constexpr std::uint32_t kFnvPrime32 = 0x01000193;
constexpr std::uint32_t kFnvOffsetBasis32 = 0x811C9DC5;

std::uint32_t HashElementKey(const charm::contracts::ElementKey& key) {
  std::uint32_t hash = kFnvOffsetBasis32;
  const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&key);

  // Hash byte by byte. Note that the memory representation depends on compiler packing,
  // but as per constraints, ElementKey must be __attribute__((packed)) for consistent hashing.
  for (std::size_t i = 0; i < sizeof(charm::contracts::ElementKey); ++i) {
    hash ^= ptr[i];
    hash *= kFnvPrime32;
  }

  return hash;
}

charm::contracts::InputElementType DetermineElementType(charm::contracts::UsagePage usage_page, charm::contracts::Usage usage) {
  // Usage Page 0x01 = Generic Desktop Controls
  if (usage_page == 0x01) {
    if (usage >= 0x30 && usage <= 0x38) {
      return charm::contracts::InputElementType::kAxis;
    }
    if (usage == 0x39) { // Hat switch
      return charm::contracts::InputElementType::kHat;
    }
  }
  // Usage Page 0x09 = Button
  else if (usage_page == 0x09) {
    return charm::contracts::InputElementType::kButton;
  }

  // Default fallback for unmapped usage
  return charm::contracts::InputElementType::kScalar;
}

}  // namespace

BuildDecodePlanResult DefaultDecodePlanBuilder::BuildDecodePlan(const BuildDecodePlanRequest& request) const {
  BuildDecodePlanResult result;

  const auto& semantic_model = request.input.semantic_model;
  DecodePlan plan;

  for (std::size_t i = 0; i < semantic_model.field_count; ++i) {
    if (plan.binding_count >= kMaxDecodeBindingsPerInterface) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kCapacityExceeded,
          .reason = 5}; // Bindings capacity exceeded
      return result;
    }

    const auto& field = semantic_model.fields[i];
    DecodeBinding& binding = plan.bindings[plan.binding_count++];

    // Construct the semantic ElementKey
    binding.element_key.vendor_id = 0; // Not available in DecodePlanInput directly, depends on USB attach
    binding.element_key.product_id = 0;
    binding.element_key.hub_path.depth = 0;
    // We bind interface level identifiers here based on what we have
    binding.element_key.interface_number = request.input.interface_number;
    binding.element_key.report_id = field.report_id;
    binding.element_key.usage_page = field.usage_page;
    binding.element_key.usage = field.usage;
    binding.element_key.collection_index = field.collection_index;
    binding.element_key.logical_index = field.logical_index;

    // Hash the key using FNV-1a
    binding.element_key_hash.value = HashElementKey(binding.element_key);

    // Determine the type
    binding.element_type = DetermineElementType(field.usage_page, field.usage);

    // Copy the decoding bounds
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
