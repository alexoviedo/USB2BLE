#include <gtest/gtest.h>
#include "charm/core/supervisor.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/status_types.hpp"

using namespace charm::core;
using namespace charm::contracts;

class SupervisorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    supervisor_ = std::make_unique<DefaultSupervisor>();
  }

  std::unique_ptr<DefaultSupervisor> supervisor_;
};

TEST_F(SupervisorTest, InitialStateIsUnknown) {
  auto state = supervisor_->GetState();
  EXPECT_EQ(state.mode, ModeState::kUnknown);
  EXPECT_EQ(state.recovery_state, RecoveryState::kNone);
}

TEST_F(SupervisorTest, TransitionToRunModeSucceeds) {
  ModeTransitionRequest req{ModeState::kRun};
  auto result = supervisor_->TransitionMode(req);
  EXPECT_EQ(result.status, ContractStatus::kOk);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.mode, ModeState::kRun);
}

TEST_F(SupervisorTest, TransitionToConfigurationModeSucceeds) {
  ModeTransitionRequest req{ModeState::kConfiguration};
  auto result = supervisor_->TransitionMode(req);
  EXPECT_EQ(result.status, ContractStatus::kOk);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.mode, ModeState::kConfiguration);
}

TEST_F(SupervisorTest, TransitionToUnknownModeFails) {
  ModeTransitionRequest req{ModeState::kRun};
  supervisor_->TransitionMode(req);

  ModeTransitionRequest bad_req{ModeState::kUnknown};
  auto result = supervisor_->TransitionMode(bad_req);
  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.mode, ModeState::kRun);
}

TEST_F(SupervisorTest, SelectProfileSucceedsWhenNoRecovery) {
  SelectProfileRequest req{.profile_id = ProfileId{42}};
  auto result = supervisor_->SelectProfile(req);
  EXPECT_EQ(result.status, ContractStatus::kOk);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.active_profile.profile_id.value, 42);
}

TEST_F(SupervisorTest, ActivateMappingBundleSucceedsWhenNoRecovery) {
  MappingBundleRef bundle_ref{};
  ActivateMappingBundleRequest req{.mapping_bundle = bundle_ref};
  auto result = supervisor_->ActivateMappingBundle(req);
  EXPECT_EQ(result.status, ContractStatus::kOk);
}

TEST_F(SupervisorTest, RequestRecoveryValidTransitions) {
  // kNone -> kRequested
  RecoveryRequest req1{.target_state = RecoveryState::kRequested};
  auto res1 = supervisor_->RequestRecovery(req1);
  EXPECT_EQ(res1.status, ContractStatus::kOk);
  EXPECT_EQ(supervisor_->GetState().recovery_state, RecoveryState::kRequested);

  // kRequested -> kRecovering
  RecoveryRequest req2{.target_state = RecoveryState::kRecovering};
  auto res2 = supervisor_->RequestRecovery(req2);
  EXPECT_EQ(res2.status, ContractStatus::kOk);
  EXPECT_EQ(supervisor_->GetState().recovery_state, RecoveryState::kRecovering);

  // kRecovering -> kNone
  RecoveryRequest req3{.target_state = RecoveryState::kNone};
  auto res3 = supervisor_->RequestRecovery(req3);
  EXPECT_EQ(res3.status, ContractStatus::kOk);
  EXPECT_EQ(supervisor_->GetState().recovery_state, RecoveryState::kNone);
}

TEST_F(SupervisorTest, RequestRecoveryInvalidTransitions) {
  // kNone -> kRecovering (invalid)
  RecoveryRequest req1{.target_state = RecoveryState::kRecovering};
  auto res1 = supervisor_->RequestRecovery(req1);
  EXPECT_EQ(res1.status, ContractStatus::kRejected);
  EXPECT_EQ(res1.fault_code.category, ErrorCategory::kInvalidState);
}

TEST_F(SupervisorTest, StateTransitionsRejectedDuringRecovery) {
  RecoveryRequest rec_req{.target_state = RecoveryState::kRequested};
  supervisor_->RequestRecovery(rec_req);

  ModeTransitionRequest mode_req{ModeState::kRun};
  auto mode_res = supervisor_->TransitionMode(mode_req);
  EXPECT_EQ(mode_res.status, ContractStatus::kRejected);
  EXPECT_EQ(mode_res.fault_code.category, ErrorCategory::kInvalidState);

  SelectProfileRequest prof_req{.profile_id = ProfileId{1}};
  auto prof_res = supervisor_->SelectProfile(prof_req);
  EXPECT_EQ(prof_res.status, ContractStatus::kRejected);
  EXPECT_EQ(prof_res.fault_code.category, ErrorCategory::kInvalidState);
}
