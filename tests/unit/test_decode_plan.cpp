#include <gtest/gtest.h>

#include "charm/core/decode_plan.hpp"

namespace {

using charm::contracts::ContractStatus;
using charm::contracts::ErrorCategory;
using charm::contracts::InputElementType;
using charm::core::BuildDecodePlanRequest;
using charm::core::DefaultDecodePlanBuilder;
using charm::core::DecodeBinding;

charm::core::FieldDescriptor MakeField(charm::contracts::UsagePage usage_page,
                                       charm::contracts::Usage usage,
                                       std::uint16_t bit_offset,
                                       std::uint16_t bit_size) {
  charm::core::FieldDescriptor field{};
  field.report_id = 1;
  field.usage_page = usage_page;
  field.usage = usage;
  field.bit_offset = bit_offset;
  field.bit_size = bit_size;
  field.logical_min = 0;
  field.logical_max = 255;
  return field;
}

}  // namespace

TEST(DecodePlanTest, BuildsPlanWithDeviceIdentityInElementKeys) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.interface_number = 1;
  request.input.vendor_id = 0x1234;
  request.input.product_id = 0x5678;
  request.input.hub_path.depth = 2;
  request.input.hub_path.ports[0] = 1;
  request.input.hub_path.ports[1] = 4;
  request.input.semantic_model.field_count = 1;
  request.input.semantic_model.fields[0] = MakeField(0x01, 0x30, 0, 8);

  const auto result = builder.BuildDecodePlan(request);
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.decode_plan.binding_count, 1u);
  const auto& binding = result.decode_plan.bindings[0];
  EXPECT_EQ(binding.element_key.vendor_id, 0x1234);
  EXPECT_EQ(binding.element_key.product_id, 0x5678);
  EXPECT_EQ(binding.element_key.hub_path.depth, 2u);
  EXPECT_NE(binding.element_key_hash.value, 0u);
}

TEST(DecodePlanTest, ClassifiesGamepadAndMouseGenericDesktopControlsAsAxesAndHat) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.semantic_model.field_count = 3;
  request.input.semantic_model.fields[0] = MakeField(0x01, 0x30, 0, 8);
  request.input.semantic_model.fields[1] = MakeField(0x01, 0x38, 8, 8);
  request.input.semantic_model.fields[2] = MakeField(0x01, 0x39, 16, 4);
  request.input.semantic_model.fields[2].logical_min = 0;
  request.input.semantic_model.fields[2].logical_max = 7;
  request.input.semantic_model.fields[2].has_null_state = true;

  const auto result = builder.BuildDecodePlan(request);
  ASSERT_EQ(result.status, ContractStatus::kOk);
  EXPECT_EQ(result.decode_plan.bindings[0].element_type, InputElementType::kAxis);
  EXPECT_EQ(result.decode_plan.bindings[1].element_type, InputElementType::kAxis);
  EXPECT_EQ(result.decode_plan.bindings[2].element_type, InputElementType::kHat);
}

TEST(DecodePlanTest, ClassifiesHotasThrottleAndPedalsAsTriggersAndAxes) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.semantic_model.field_count = 4;
  request.input.semantic_model.fields[0] = MakeField(0x02, 0xBA, 0, 16);   // Rudder
  request.input.semantic_model.fields[1] = MakeField(0x02, 0xBB, 16, 16);  // Throttle
  request.input.semantic_model.fields[2] = MakeField(0x02, 0xC4, 32, 16);  // Accelerator
  request.input.semantic_model.fields[3] = MakeField(0x02, 0xC5, 48, 16);  // Brake
  for (std::size_t i = 0; i < request.input.semantic_model.field_count; ++i) {
    request.input.semantic_model.fields[i].logical_max = 1023;
  }

  const auto result = builder.BuildDecodePlan(request);
  ASSERT_EQ(result.status, ContractStatus::kOk);
  EXPECT_EQ(result.decode_plan.bindings[0].element_type, InputElementType::kAxis);
  EXPECT_EQ(result.decode_plan.bindings[1].element_type, InputElementType::kTrigger);
  EXPECT_EQ(result.decode_plan.bindings[2].element_type, InputElementType::kTrigger);
  EXPECT_EQ(result.decode_plan.bindings[3].element_type, InputElementType::kTrigger);
}

TEST(DecodePlanTest, PreservesKeyboardArrayMetadataForDecoderAndRuntimeExpansion) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.interface_number = 2;
  request.input.semantic_model.field_count = 2;

  auto modifier = MakeField(0x07, 0xE0, 0, 1);
  modifier.logical_max = 1;
  request.input.semantic_model.fields[0] = modifier;

  auto key_slot = MakeField(0x07, 0, 8, 8);
  key_slot.is_array = true;
  key_slot.has_usage_range = true;
  key_slot.usage_min = 0;
  key_slot.usage_max = 0x65;
  key_slot.logical_max = 0x65;
  request.input.semantic_model.fields[1] = key_slot;

  const auto result = builder.BuildDecodePlan(request);
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.decode_plan.binding_count, 2u);

  const auto& modifier_binding = result.decode_plan.bindings[0];
  EXPECT_EQ(modifier_binding.element_type, InputElementType::kButton);
  EXPECT_FALSE(modifier_binding.is_array);

  const auto& array_binding = result.decode_plan.bindings[1];
  EXPECT_EQ(array_binding.element_type, InputElementType::kButton);
  EXPECT_TRUE(array_binding.is_array);
  EXPECT_TRUE(array_binding.has_usage_range);
  EXPECT_EQ(array_binding.usage_min, 0u);
  EXPECT_EQ(array_binding.usage_max, 0x65u);
}

TEST(DecodePlanTest, RejectsZeroSizedFields) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.semantic_model.field_count = 1;
  request.input.semantic_model.fields[0] = MakeField(0x01, 0x30, 0, 0);

  const auto result = builder.BuildDecodePlan(request);
  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);
}

TEST(DecodePlanTest, RejectsTooManyBindings) {
  DefaultDecodePlanBuilder builder;
  BuildDecodePlanRequest request{};
  request.input.semantic_model.field_count =
      charm::core::kMaxDecodeBindingsPerInterface + 1;

  const auto result = builder.BuildDecodePlan(request);
  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kCapacityExceeded);
}
