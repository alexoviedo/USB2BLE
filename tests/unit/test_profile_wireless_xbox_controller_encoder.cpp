#include <gtest/gtest.h>

#include <cstring>

#include "charm/core/profile_manager.hpp"

namespace {

struct __attribute__((packed)) WirelessXboxControllerReport {
  std::uint16_t buttons{0};
  std::uint8_t dpad{0};
  std::uint8_t left_trigger{0};
  std::uint8_t right_trigger{0};
  std::int16_t left_x{0};
  std::int16_t left_y{0};
  std::int16_t right_x{0};
  std::int16_t right_y{0};
};

}  // namespace

class ProfileWirelessXboxControllerEncoderTest : public ::testing::Test {
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

TEST_F(ProfileWirelessXboxControllerEncoderTest, CapabilitiesAreCorrect) {
  const auto result = charm::core::profile_wireless_xbox_controller::GetCapabilities();

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.descriptor.profile_id.value, 2u);
  EXPECT_STREQ(result.descriptor.name, "Wireless Xbox Controller");
  EXPECT_EQ(result.descriptor.report_id, 2u);
  EXPECT_EQ(result.descriptor.report_size, sizeof(WirelessXboxControllerReport));
  EXPECT_GT(result.descriptor.capability_count, 0u);
}

TEST_F(ProfileWirelessXboxControllerEncoderTest, NullStateFails) {
  charm::core::EncodeLogicalStateRequest bad_req = req;
  bad_req.logical_state = nullptr;

  const auto result = charm::core::profile_wireless_xbox_controller::Encode(bad_req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kInvalidRequest);
}

TEST_F(ProfileWirelessXboxControllerEncoderTest, NullHatProducesZeroedReport) {
  state.hat.value = 8;

  const auto result = charm::core::profile_wireless_xbox_controller::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(result.report.report_id, 2u);
  ASSERT_EQ(result.report.size, sizeof(WirelessXboxControllerReport));

  WirelessXboxControllerReport report{};
  std::memcpy(&report, result.report.bytes, sizeof(report));

  EXPECT_EQ(report.buttons, 0u);
  EXPECT_EQ(report.dpad, 0u);
  EXPECT_EQ(report.left_trigger, 0u);
  EXPECT_EQ(report.right_trigger, 0u);
  EXPECT_EQ(report.left_x, 0);
  EXPECT_EQ(report.left_y, 0);
  EXPECT_EQ(report.right_x, 0);
  EXPECT_EQ(report.right_y, 0);
}

TEST_F(ProfileWirelessXboxControllerEncoderTest, HidHatUpMapsToUpDpadBit) {
  state.hat.value = 0;

  const auto result = charm::core::profile_wireless_xbox_controller::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);

  WirelessXboxControllerReport report{};
  std::memcpy(&report, result.report.bytes, sizeof(report));

  EXPECT_EQ(report.dpad, static_cast<std::uint8_t>(1u << 0));
}

TEST_F(ProfileWirelessXboxControllerEncoderTest, EncodesButtonsDpadAxesAndTriggers) {
  state.buttons[0].pressed = true;
  state.buttons[3].pressed = true;
  state.buttons[12].pressed = true;
  state.hat.value = 1;  // up + right
  state.axes[0].value = 127;
  state.axes[1].value = -128;
  state.axes[2].value = 64;
  state.axes[3].value = -64;
  state.left_trigger.value = 200;
  state.right_trigger.value = 250;

  const auto result = charm::core::profile_wireless_xbox_controller::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);

  WirelessXboxControllerReport report{};
  std::memcpy(&report, result.report.bytes, sizeof(report));

  EXPECT_EQ(report.buttons, static_cast<std::uint16_t>((1u << 0) | (1u << 3) | (1u << 12)));
  EXPECT_EQ(report.dpad, static_cast<std::uint8_t>((1u << 0) | (1u << 3)));
  EXPECT_EQ(report.left_trigger, 200u);
  EXPECT_EQ(report.right_trigger, 250u);
  EXPECT_EQ(report.left_x, 32512);
  EXPECT_EQ(report.left_y, -32768);
  EXPECT_EQ(report.right_x, 16384);
  EXPECT_EQ(report.right_y, -16384);
}

TEST_F(ProfileWirelessXboxControllerEncoderTest, ClampsValuesCorrectly) {
  state.axes[0].value = 300;
  state.axes[1].value = -500;
  state.left_trigger.value = 999;
  state.right_trigger.value = 400;
  state.hat.value = 9;

  const auto result = charm::core::profile_wireless_xbox_controller::Encode(req);

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);

  WirelessXboxControllerReport report{};
  std::memcpy(&report, result.report.bytes, sizeof(report));

  EXPECT_EQ(report.left_x, 32512);
  EXPECT_EQ(report.left_y, -32768);
  EXPECT_EQ(report.left_trigger, 255u);
  EXPECT_EQ(report.right_trigger, 255u);
  EXPECT_EQ(report.dpad, 0u);
}
