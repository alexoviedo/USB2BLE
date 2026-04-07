#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "charm/core/hid_semantic_model.hpp"

namespace {

using charm::contracts::ContractStatus;
using charm::contracts::ErrorCategory;
using charm::core::CollectionKind;
using charm::core::DefaultHidDescriptorParser;
using charm::core::ParseDescriptorRequest;

std::vector<std::uint8_t> MakeGamepadDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x05,        // Usage (Game Pad)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x09,        // Usage Page (Button)
      0x19, 0x01,        // Usage Minimum (1)
      0x29, 0x04,        // Usage Maximum (4)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x01,        // Logical Maximum (1)
      0x75, 0x01,        // Report Size (1)
      0x95, 0x04,        // Report Count (4)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0x75, 0x04,        // Report Size (4)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x03,        // Input (Const,Var,Abs)
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x39,        // Usage (Hat switch)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x07,        // Logical Maximum (7)
      0x35, 0x00,        // Physical Minimum (0)
      0x46, 0x3B, 0x01,  // Physical Maximum (315)
      0x75, 0x04,        // Report Size (4)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x42,        // Input (Data,Var,Abs,Null)
      0x75, 0x04,        // Report Size (4)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x03,        // Input (Const,Var,Abs)
      0x09, 0x30,        // Usage (X)
      0x09, 0x31,        // Usage (Y)
      0x15, 0x00,        // Logical Minimum (0)
      0x26, 0xFF, 0x00,  // Logical Maximum (255)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x02,        // Report Count (2)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0xC0,              // End Collection
  };
}

std::vector<std::uint8_t> MakeHotasDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x04,        // Usage (Joystick)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x02,        // Usage Page (Simulation Controls)
      0x09, 0xBA,        // Usage (Rudder)
      0x09, 0xBB,        // Usage (Throttle)
      0x09, 0xC4,        // Usage (Accelerator)
      0x09, 0xC5,        // Usage (Brake)
      0x15, 0x00,        // Logical Minimum (0)
      0x26, 0xFF, 0x03,  // Logical Maximum (1023)
      0x75, 0x10,        // Report Size (16)
      0x95, 0x04,        // Report Count (4)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0xC0,              // End Collection
  };
}

std::vector<std::uint8_t> MakeKeyboardDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x06,        // Usage (Keyboard)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x07,        // Usage Page (Keyboard)
      0x19, 0xE0,        // Usage Minimum (224)
      0x29, 0xE7,        // Usage Maximum (231)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x01,        // Logical Maximum (1)
      0x75, 0x01,        // Report Size (1)
      0x95, 0x08,        // Report Count (8)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x03,        // Input (Const,Var,Abs)
      0x19, 0x00,        // Usage Minimum (0)
      0x29, 0x65,        // Usage Maximum (101)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x65,        // Logical Maximum (101)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x06,        // Report Count (6)
      0x81, 0x00,        // Input (Data,Array,Abs)
      0xC0,              // End Collection
  };
}

std::vector<std::uint8_t> MakeMouseDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x02,        // Usage (Mouse)
      0xA1, 0x01,        // Collection (Application)
      0x09, 0x01,        // Usage (Pointer)
      0xA1, 0x00,        // Collection (Physical)
      0x05, 0x09,        // Usage Page (Button)
      0x19, 0x01,        // Usage Minimum (1)
      0x29, 0x03,        // Usage Maximum (3)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x01,        // Logical Maximum (1)
      0x75, 0x01,        // Report Size (1)
      0x95, 0x03,        // Report Count (3)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0x75, 0x05,        // Report Size (5)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x03,        // Input (Const,Var,Abs)
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x30,        // Usage (X)
      0x09, 0x31,        // Usage (Y)
      0x09, 0x38,        // Usage (Wheel)
      0x15, 0x81,        // Logical Minimum (-127)
      0x25, 0x7F,        // Logical Maximum (127)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x03,        // Report Count (3)
      0x81, 0x06,        // Input (Data,Var,Rel)
      0xC0,              // End Collection
      0xC0,              // End Collection
  };
}

charm::core::ParseDescriptorResult Parse(
    const std::vector<std::uint8_t>& descriptor) {
  DefaultHidDescriptorParser parser;
  ParseDescriptorRequest request{};
  request.descriptor.bytes = descriptor.data();
  request.descriptor.size = descriptor.size();
  return parser.ParseDescriptor(request);
}

}  // namespace

TEST(HidSemanticModelTest, EmptyDescriptorIsRejected) {
  DefaultHidDescriptorParser parser;
  ParseDescriptorRequest request{};

  const auto result = parser.ParseDescriptor(request);
  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);
}

TEST(HidSemanticModelTest, ParsesGamepadButtonsHatAndAxes) {
  const auto result = Parse(MakeGamepadDescriptor());
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.semantic_model.collection_count, 1u);
  EXPECT_EQ(result.semantic_model.collections[0].kind, CollectionKind::kApplication);
  ASSERT_EQ(result.semantic_model.field_count, 7u);

  EXPECT_EQ(result.semantic_model.fields[0].usage_page, 0x09);
  EXPECT_EQ(result.semantic_model.fields[0].usage, 1u);
  EXPECT_FALSE(result.semantic_model.fields[0].is_array);

  EXPECT_EQ(result.semantic_model.fields[4].usage_page, 0x01);
  EXPECT_EQ(result.semantic_model.fields[4].usage, 0x39);
  EXPECT_TRUE(result.semantic_model.fields[4].has_null_state);
  EXPECT_EQ(result.semantic_model.fields[4].logical_max, 7);

  EXPECT_EQ(result.semantic_model.fields[5].usage, 0x30);
  EXPECT_EQ(result.semantic_model.fields[5].logical_min, 0);
  EXPECT_EQ(result.semantic_model.fields[5].logical_max, 255);
  EXPECT_EQ(result.semantic_model.fields[6].usage, 0x31);
}

TEST(HidSemanticModelTest, ParsesHotasSimulationControlsWithLogicalRanges) {
  const auto result = Parse(MakeHotasDescriptor());
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.semantic_model.field_count, 4u);

  EXPECT_EQ(result.semantic_model.fields[0].usage_page, 0x02);
  EXPECT_EQ(result.semantic_model.fields[0].usage, 0xBA);
  EXPECT_EQ(result.semantic_model.fields[0].logical_max, 1023);
  EXPECT_FALSE(result.semantic_model.fields[0].is_relative);

  EXPECT_EQ(result.semantic_model.fields[1].usage, 0xBB);
  EXPECT_EQ(result.semantic_model.fields[2].usage, 0xC4);
  EXPECT_EQ(result.semantic_model.fields[3].usage, 0xC5);
}

TEST(HidSemanticModelTest, ParsesKeyboardModifierBitsAndKeyArray) {
  const auto result = Parse(MakeKeyboardDescriptor());
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.semantic_model.field_count, 14u);

  EXPECT_EQ(result.semantic_model.fields[0].usage_page, 0x07);
  EXPECT_EQ(result.semantic_model.fields[0].usage, 0xE0);
  EXPECT_FALSE(result.semantic_model.fields[0].is_array);

  const auto& key_slot = result.semantic_model.fields[8];
  EXPECT_EQ(key_slot.usage_page, 0x07);
  EXPECT_TRUE(key_slot.is_array);
  EXPECT_TRUE(key_slot.has_usage_range);
  EXPECT_EQ(key_slot.usage_min, 0u);
  EXPECT_EQ(key_slot.usage_max, 0x65u);
  EXPECT_EQ(key_slot.bit_size, 8u);
}

TEST(HidSemanticModelTest, ParsesMouseButtonsAndRelativeAxes) {
  const auto result = Parse(MakeMouseDescriptor());
  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.semantic_model.collection_count, 2u);
  ASSERT_EQ(result.semantic_model.field_count, 6u);

  EXPECT_EQ(result.semantic_model.fields[0].usage_page, 0x09);
  EXPECT_EQ(result.semantic_model.fields[0].usage, 1u);
  EXPECT_FALSE(result.semantic_model.fields[0].is_relative);

  EXPECT_EQ(result.semantic_model.fields[3].usage_page, 0x01);
  EXPECT_EQ(result.semantic_model.fields[3].usage, 0x30);
  EXPECT_TRUE(result.semantic_model.fields[3].is_relative);
  EXPECT_TRUE(result.semantic_model.fields[3].is_signed);

  EXPECT_EQ(result.semantic_model.fields[5].usage, 0x38);
  EXPECT_TRUE(result.semantic_model.fields[5].is_relative);
}

TEST(HidSemanticModelTest, MalformedDescriptorIsRejected) {
  DefaultHidDescriptorParser parser;
  ParseDescriptorRequest request{};
  const std::vector<std::uint8_t> descriptor = {0x05, 0x01, 0x09};
  request.descriptor.bytes = descriptor.data();
  request.descriptor.size = descriptor.size();

  const auto result = parser.ParseDescriptor(request);
  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kContractViolation);
}
