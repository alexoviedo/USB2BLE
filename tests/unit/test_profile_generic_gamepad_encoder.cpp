#include <gtest/gtest.h>

#include <cstring>

#include "charm/core/profile_manager.hpp"

namespace {
// Struct definition duplicated to read the packed struct in test
struct __attribute__((packed)) GenericGamepadReport {
  std::uint8_t buttons_low{0};
  std::uint8_t buttons_high{0};
  std::uint8_t hat{0};
  std::int8_t left_x{0};
  std::int8_t left_y{0};
  std::int8_t right_x{0};
  std::int8_t right_y{0};
  std::uint8_t left_trigger{0};
  std::uint8_t right_trigger{0};
};
}  // namespace

class ProfileGenericGamepadEncoderTest : public ::testing::Test {
 protected:
  charm::contracts::LogicalGamepadState state{};
  std::uint8_t buffer[64]{};
  charm::core::EncodeLogicalStateRequest req{};

  void SetUp() override {
    req.logical_state = &state;
    req.output_buffer = buffer;
    req.output_buffer_capacity = sizeof(buffer);
  }
};

TEST_F(ProfileGenericGamepadEncoderTest, CapabilitiesAreCorrect) {
  auto result = charm::core::profile_generic_gamepad::GetCapabilities();

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.descriptor.profile_id.value, 1u);
  EXPECT_STREQ(result.descriptor.name, "Generic Gamepad");
  EXPECT_GT(result.descriptor.name_length, 0u);
  EXPECT_GT(result.descriptor.capability_count, 0u);
  EXPECT_NE(result.descriptor.capabilities, nullptr);
}

TEST_F(ProfileGenericGamepadEncoderTest, NullStateFails) {
  charm::core::EncodeLogicalStateRequest bad_req = req;
  bad_req.logical_state = nullptr;
  auto result = charm::core::profile_generic_gamepad::Encode(bad_req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kInvalidRequest);
}

TEST_F(ProfileGenericGamepadEncoderTest, DefaultStateProducesZeroedReport) {
  auto result = charm::core::profile_generic_gamepad::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(result.report.report_id, 1u);
  ASSERT_EQ(result.report.size, sizeof(GenericGamepadReport));

  GenericGamepadReport report;
  std::memcpy(&report, result.report.bytes, sizeof(GenericGamepadReport));

  EXPECT_EQ(report.buttons_low, 0);
  EXPECT_EQ(report.buttons_high, 0);
  EXPECT_EQ(report.hat, 0);
  EXPECT_EQ(report.left_x, 0);
  EXPECT_EQ(report.left_y, 0);
  EXPECT_EQ(report.right_x, 0);
  EXPECT_EQ(report.right_y, 0);
  EXPECT_EQ(report.left_trigger, 0);
  EXPECT_EQ(report.right_trigger, 0);
}

TEST_F(ProfileGenericGamepadEncoderTest, EncodesAllFieldsCorrectly) {
  state.buttons[0].pressed = true;
  state.buttons[15].pressed = true;
  state.hat.value = 5;
  state.axes[0].value = 100;    // Lx
  state.axes[1].value = -100;   // Ly
  state.axes[2].value = 50;     // Rx
  state.axes[3].value = -50;    // Ry
  state.left_trigger.value = 200;
  state.right_trigger.value = 250;

  auto result = charm::core::profile_generic_gamepad::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);

  GenericGamepadReport report;
  std::memcpy(&report, result.report.bytes, sizeof(GenericGamepadReport));

  EXPECT_EQ(report.buttons_low, 1);
  EXPECT_EQ(report.buttons_high, 128);
  EXPECT_EQ(report.hat, 5);
  EXPECT_EQ(report.left_x, 100);
  EXPECT_EQ(report.left_y, -100);
  EXPECT_EQ(report.right_x, 50);
  EXPECT_EQ(report.right_y, -50);
  EXPECT_EQ(report.left_trigger, 200);
  EXPECT_EQ(report.right_trigger, 250);
}

TEST_F(ProfileGenericGamepadEncoderTest, ClampsValuesCorrectly) {
  state.axes[0].value = 300;     // Clamp to 127
  state.axes[1].value = -300;    // Clamp to -128
  state.left_trigger.value = 500; // Clamp to 255

  auto result = charm::core::profile_generic_gamepad::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);

  GenericGamepadReport report;
  std::memcpy(&report, result.report.bytes, sizeof(GenericGamepadReport));

  EXPECT_EQ(report.left_x, 127);
  EXPECT_EQ(report.left_y, -128);
  EXPECT_EQ(report.left_trigger, 255);
}
