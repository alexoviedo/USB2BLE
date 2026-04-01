#include <gtest/gtest.h>

#include "charm/core/profile_manager.hpp"

class ProfileManagerTest : public ::testing::Test {
 protected:
  charm::core::CanonicalProfileManager manager{};
};

TEST_F(ProfileManagerTest, SelectKnownProfileSucceeds) {
  charm::contracts::SelectProfileRequest req{};
  req.profile_id.value = 1;

  auto result = manager.SelectProfile(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
}

TEST_F(ProfileManagerTest, SelectUnknownProfileFails) {
  charm::contracts::SelectProfileRequest req{};
  req.profile_id.value = 999;

  auto result = manager.SelectProfile(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kUnsupportedCapability);
}

TEST_F(ProfileManagerTest, EncodeWithoutSelectionFails) {
  charm::core::EncodeLogicalStateRequest req{};
  req.profile_id.value = 1;
  charm::contracts::LogicalGamepadState state{};
  req.logical_state = &state;

  auto result = manager.EncodeLogicalState(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kInvalidState);
}

TEST_F(ProfileManagerTest, EncodeWithSelectionSucceeds) {
  charm::contracts::SelectProfileRequest sel_req{};
  sel_req.profile_id.value = 1;
  ASSERT_EQ(manager.SelectProfile(sel_req).status, charm::contracts::ContractStatus::kOk);

  charm::core::EncodeLogicalStateRequest enc_req{};
  enc_req.profile_id.value = 1;
  charm::contracts::LogicalGamepadState state{};
  std::uint8_t buffer[64];
  enc_req.logical_state = &state;
  enc_req.output_buffer = buffer;
  enc_req.output_buffer_capacity = sizeof(buffer);

  auto result = manager.EncodeLogicalState(enc_req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.report.report_id, 1u);
}

TEST_F(ProfileManagerTest, GetCapabilitiesKnownProfile) {
  charm::core::GetProfileCapabilitiesRequest req{};
  req.profile_id.value = 1;

  auto result = manager.GetProfileCapabilities(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(result.descriptor.profile_id.value, 1u);
}

TEST_F(ProfileManagerTest, GetCapabilitiesUnknownProfile) {
  charm::core::GetProfileCapabilitiesRequest req{};
  req.profile_id.value = 999;

  auto result = manager.GetProfileCapabilities(req);

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kUnsupportedCapability);
}
