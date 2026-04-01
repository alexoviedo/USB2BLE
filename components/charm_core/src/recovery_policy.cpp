#include "charm/core/recovery_policy.hpp"

namespace charm::core {

DefaultRecoveryPolicy::DefaultRecoveryPolicy(Supervisor& supervisor)
    : supervisor_(supervisor) {}

void DefaultRecoveryPolicy::Publish(const charm::contracts::ControlPlaneEvent& /*event*/) {
  // Policy doesn't react to standard control plane events yet
}

void DefaultRecoveryPolicy::Publish(const charm::contracts::FaultEvent& event) {
  if (event.severity == charm::contracts::FaultSeverity::kError ||
      event.severity == charm::contracts::FaultSeverity::kFatal) {

    charm::contracts::FaultRecordRef fault_record{
        .fault_code = event.fault_code,
        .severity = event.severity,
        .timestamp = event.timestamp
    };

    supervisor_.SetLastFault(fault_record);

    charm::contracts::RecoveryRequest req{
        .target_state = charm::contracts::RecoveryState::kRequested
    };
    supervisor_.RequestRecovery(req);
  }
}

void DefaultRecoveryPolicy::Publish(const charm::contracts::AdapterStatusEvent& /*event*/) {
  // Policy doesn't react to adapter status events yet
}

}  // namespace charm::core
