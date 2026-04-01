#include <gtest/gtest.h>

#include "charm/core/logical_state.hpp"

namespace charm::core {
namespace {

using contracts::ContractStatus;
using contracts::ErrorCategory;
using contracts::ProfileId;
using contracts::Timestamp;

class LogicalStateTest : public ::testing::Test {
 protected:
  ProfileId test_profile_{123};
  CanonicalLogicalStateStore store_{test_profile_};
};

TEST_F(LogicalStateTest, InitializationReturnsDefaults) {
  GetLogicalStateRequest req{test_profile_};
  auto result = store_.GetLogicalState(req);

  ASSERT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.snapshot.profile_id.value, test_profile_.value);
  ASSERT_EQ(result.snapshot.timestamp.ticks, 0);
  ASSERT_NE(result.snapshot.state, nullptr);

  // Check defaults
  for (const auto& axis : result.snapshot.state->axes) {
    EXPECT_EQ(axis.value, 0);
  }
  for (const auto& btn : result.snapshot.state->buttons) {
    EXPECT_FALSE(btn.pressed);
  }
  EXPECT_EQ(result.snapshot.state->left_trigger.value, 0);
  EXPECT_EQ(result.snapshot.state->right_trigger.value, 0);
  EXPECT_EQ(result.snapshot.state->hat.value, 0);
}

TEST_F(LogicalStateTest, RejectsMismatchedProfileId) {
  GetLogicalStateRequest req{ProfileId{999}};
  auto result = store_.GetLogicalState(req);

  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kContractViolation);
  EXPECT_EQ(result.snapshot.state, nullptr);
}

TEST_F(LogicalStateTest, MutableStateUpdatesTimestampAndValues) {
  Timestamp current_time{456};
  auto& mutable_state = store_.GetMutableState(current_time);

  // Modify state
  mutable_state.axes[0].value = 100;
  mutable_state.buttons[1].pressed = true;

  // Read back and verify
  GetLogicalStateRequest req{test_profile_};
  auto result = store_.GetLogicalState(req);

  ASSERT_EQ(result.status, ContractStatus::kOk);
  EXPECT_EQ(result.snapshot.timestamp.ticks, 456);
  ASSERT_NE(result.snapshot.state, nullptr);
  EXPECT_EQ(result.snapshot.state->axes[0].value, 100);
  EXPECT_TRUE(result.snapshot.state->buttons[1].pressed);
}

TEST_F(LogicalStateTest, ResetClearsState) {
  // Mutate state first
  auto& mutable_state = store_.GetMutableState(Timestamp{789});
  mutable_state.axes[2].value = 500;
  mutable_state.buttons[5].pressed = true;

  // Reset
  ResetLogicalStateRequest reset_req{};
  auto reset_result = store_.ResetLogicalState(reset_req);
  EXPECT_EQ(reset_result.status, ContractStatus::kOk);

  // Verify it's cleared
  GetLogicalStateRequest req{test_profile_};
  auto result = store_.GetLogicalState(req);

  ASSERT_EQ(result.status, ContractStatus::kOk);
  EXPECT_EQ(result.snapshot.state->axes[2].value, 0);
  EXPECT_FALSE(result.snapshot.state->buttons[5].pressed);
  // Timestamp should be preserved or whatever we didn't reset it (it only reset the state object)
  // Our implementation doesn't clear last_update_time_. The requirement doesn't specify clearing time.
}

}  // namespace
}  // namespace charm::core
