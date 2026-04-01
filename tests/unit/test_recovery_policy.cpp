#include <gtest/gtest.h>
#include "charm/core/recovery_policy.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/events.hpp"

using namespace charm::core;
using namespace charm::contracts;

class RecoveryPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    supervisor_ = std::make_unique<DefaultSupervisor>();
    policy_ = std::make_unique<DefaultRecoveryPolicy>(*supervisor_);
  }

  std::unique_ptr<DefaultSupervisor> supervisor_;
  std::unique_ptr<DefaultRecoveryPolicy> policy_;
};

TEST_F(RecoveryPolicyTest, InfoFaultDoesNotTriggerRecovery) {
  FaultEvent event{
      .domain = FaultDomain::kContract,
      .fault_code = {ErrorCategory::kInvalidState, 1},
      .severity = FaultSeverity::kInfo,
      .source = EventSource::kSupervisor,
      .timestamp = Timestamp{100}
  };

  policy_->Publish(event);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.recovery_state, RecoveryState::kNone);
  EXPECT_EQ(state.last_fault.fault_code.category, ErrorCategory::kContractViolation); // default
}

TEST_F(RecoveryPolicyTest, WarningFaultDoesNotTriggerRecovery) {
  FaultEvent event{
      .domain = FaultDomain::kContract,
      .fault_code = {ErrorCategory::kInvalidState, 1},
      .severity = FaultSeverity::kWarning,
      .source = EventSource::kSupervisor,
      .timestamp = Timestamp{100}
  };

  policy_->Publish(event);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.recovery_state, RecoveryState::kNone);
}

TEST_F(RecoveryPolicyTest, ErrorFaultTriggersRecovery) {
  FaultEvent event{
      .domain = FaultDomain::kAdapter,
      .fault_code = {ErrorCategory::kAdapterFailure, 42},
      .severity = FaultSeverity::kError,
      .source = EventSource::kUsbHost,
      .timestamp = Timestamp{200}
  };

  policy_->Publish(event);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.recovery_state, RecoveryState::kRequested);
  EXPECT_EQ(state.last_fault.fault_code.category, ErrorCategory::kAdapterFailure);
  EXPECT_EQ(state.last_fault.fault_code.reason, 42);
  EXPECT_EQ(state.last_fault.severity, FaultSeverity::kError);
  EXPECT_EQ(state.last_fault.timestamp.ticks, 200);
}

TEST_F(RecoveryPolicyTest, FatalFaultTriggersRecovery) {
  FaultEvent event{
      .domain = FaultDomain::kTransport,
      .fault_code = {ErrorCategory::kTransportFailure, 99},
      .severity = FaultSeverity::kFatal,
      .source = EventSource::kBleTransport,
      .timestamp = Timestamp{300}
  };

  policy_->Publish(event);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.recovery_state, RecoveryState::kRequested);
  EXPECT_EQ(state.last_fault.fault_code.category, ErrorCategory::kTransportFailure);
  EXPECT_EQ(state.last_fault.fault_code.reason, 99);
  EXPECT_EQ(state.last_fault.severity, FaultSeverity::kFatal);
  EXPECT_EQ(state.last_fault.timestamp.ticks, 300);
}

TEST_F(RecoveryPolicyTest, OtherEventsIgnored) {
  ControlPlaneEvent cp_event{};
  policy_->Publish(cp_event);

  AdapterStatusEvent as_event{};
  policy_->Publish(as_event);

  auto state = supervisor_->GetState();
  EXPECT_EQ(state.recovery_state, RecoveryState::kNone);
}
