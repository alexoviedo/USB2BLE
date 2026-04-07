#include <gtest/gtest.h>

#include "charm/core/profile_manager.hpp"

class ProfileManagerTest : public ::testing::Test {
 protected:
  charm::core::CanonicalProfileManager manager{};
  charm::contracts::LogicalGamepadState state{};
  std::uint8_t buffer[64]{};
};

TEST_F(ProfileManagerTest, SupportedProfileSetIncludesGenericAndXbox) {
  const auto result = manager.GetSupportedProfiles({});

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(result.descriptor_count, 2u);

  EXPECT_EQ(result.descriptors[0].profile_id.value,
            charm::core::kGenericBleGamepadProfileId.value);
  EXPECT_STREQ(result.descriptors[0].name, "Generic BLE Gamepad");
  EXPECT_EQ(result.descriptors[0].report_id, 1u);

  EXPECT_EQ(result.descriptors[1].profile_id.value,
            charm::core::kWirelessXboxControllerProfileId.value);
  EXPECT_STREQ(result.descriptors[1].name, "Wireless Xbox Controller");
  EXPECT_EQ(result.descriptors[1].report_id, 2u);
}

TEST_F(ProfileManagerTest, SelectGenericProfileSucceeds) {
  charm::contracts::SelectProfileRequest req{};
  req.profile_id = charm::core::kGenericBleGamepadProfileId;

  const auto result = manager.SelectProfile(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
}

TEST_F(ProfileManagerTest, SelectXboxProfileSucceeds) {
  charm::contracts::SelectProfileRequest req{};
  req.profile_id = charm::core::kWirelessXboxControllerProfileId;

  const auto result = manager.SelectProfile(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
}

TEST_F(ProfileManagerTest, SelectUnknownProfileFails) {
  charm::contracts::SelectProfileRequest req{};
  req.profile_id.value = 999;

  const auto result = manager.SelectProfile(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kUnsupportedCapability);
}

TEST_F(ProfileManagerTest, EncodeWithoutSelectionFails) {
  charm::core::EncodeLogicalStateRequest req{};
  req.profile_id = charm::core::kGenericBleGamepadProfileId;
  req.logical_state = &state;
  req.output_buffer = buffer;
  req.output_buffer_capacity = sizeof(buffer);

  const auto result = manager.EncodeLogicalState(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kInvalidState);
}

TEST_F(ProfileManagerTest, EncodeWithSelectedGenericProfileSucceeds) {
  ASSERT_EQ(manager.SelectProfile({.profile_id = charm::core::kGenericBleGamepadProfileId}).status,
            charm::contracts::ContractStatus::kOk);

  charm::core::EncodeLogicalStateRequest req{};
  req.profile_id = charm::core::kGenericBleGamepadProfileId;
  req.logical_state = &state;
  req.output_buffer = buffer;
  req.output_buffer_capacity = sizeof(buffer);

  const auto result = manager.EncodeLogicalState(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.report.report_id, 1u);
}

TEST_F(ProfileManagerTest, EncodeWithSelectedXboxProfileSucceeds) {
  ASSERT_EQ(manager.SelectProfile({.profile_id = charm::core::kWirelessXboxControllerProfileId}).status,
            charm::contracts::ContractStatus::kOk);

  charm::core::EncodeLogicalStateRequest req{};
  req.profile_id = charm::core::kWirelessXboxControllerProfileId;
  req.logical_state = &state;
  req.output_buffer = buffer;
  req.output_buffer_capacity = sizeof(buffer);

  const auto result = manager.EncodeLogicalState(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.report.report_id, 2u);
}

TEST_F(ProfileManagerTest, EncodeFailsWhenRequestProfileDoesNotMatchSelection) {
  ASSERT_EQ(manager.SelectProfile({.profile_id = charm::core::kGenericBleGamepadProfileId}).status,
            charm::contracts::ContractStatus::kOk);

  charm::core::EncodeLogicalStateRequest req{};
  req.profile_id = charm::core::kWirelessXboxControllerProfileId;
  req.logical_state = &state;
  req.output_buffer = buffer;
  req.output_buffer_capacity = sizeof(buffer);

  const auto result = manager.EncodeLogicalState(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kInvalidState);
}

TEST_F(ProfileManagerTest, GetCapabilitiesForGenericProfile) {
  const auto result = manager.GetProfileCapabilities(
      {.profile_id = charm::core::kGenericBleGamepadProfileId});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.descriptor.profile_id.value,
            charm::core::kGenericBleGamepadProfileId.value);
  EXPECT_STREQ(result.descriptor.name, "Generic BLE Gamepad");
}

TEST_F(ProfileManagerTest, GetCapabilitiesForXboxProfile) {
  const auto result = manager.GetProfileCapabilities(
      {.profile_id = charm::core::kWirelessXboxControllerProfileId});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.descriptor.profile_id.value,
            charm::core::kWirelessXboxControllerProfileId.value);
  EXPECT_STREQ(result.descriptor.name, "Wireless Xbox Controller");
}

TEST_F(ProfileManagerTest, GetCapabilitiesUnknownProfileFails) {
  charm::core::GetProfileCapabilitiesRequest req{};
  req.profile_id.value = 999;

  const auto result = manager.GetProfileCapabilities(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kUnsupportedCapability);
}
