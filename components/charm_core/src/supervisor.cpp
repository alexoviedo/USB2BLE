#include "charm/core/supervisor.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::core {

charm::contracts::StartResult DefaultSupervisor::Start(const charm::contracts::StartRequest& /*request*/) {
  return charm::contracts::StartResult{
      .status = charm::contracts::ContractStatus::kOk,
      .fault_code = {charm::contracts::ErrorCategory::kInvalidState, 0}}; // kInvalidState since kNone is not an enum value
}

charm::contracts::StopResult DefaultSupervisor::Stop(const charm::contracts::StopRequest& /*request*/) {
  return charm::contracts::StopResult{
      .status = charm::contracts::ContractStatus::kOk,
      .fault_code = {charm::contracts::ErrorCategory::kInvalidState, 0}};
}

charm::contracts::ModeTransitionResult DefaultSupervisor::TransitionMode(const charm::contracts::ModeTransitionRequest& request) {
  charm::contracts::ModeTransitionResult result{};

  if (state_.recovery_state != charm::contracts::RecoveryState::kNone) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
      return result;
  }

  if (request.target_mode == charm::contracts::ModeState::kUnknown) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code.category = charm::contracts::ErrorCategory::kInvalidRequest;
      return result;
  }

  if (request.target_mode == state_.mode) {
      result.status = charm::contracts::ContractStatus::kOk;
      return result;
  }

  state_.mode = request.target_mode;
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

charm::contracts::ActivateMappingBundleResult DefaultSupervisor::ActivateMappingBundle(const charm::contracts::ActivateMappingBundleRequest& request) {
  charm::contracts::ActivateMappingBundleResult result{};

  if (state_.recovery_state != charm::contracts::RecoveryState::kNone) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
      return result;
  }

  state_.active_mapping_bundle.mapping_bundle = request.mapping_bundle;
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

charm::contracts::SelectProfileResult DefaultSupervisor::SelectProfile(const charm::contracts::SelectProfileRequest& request) {
  charm::contracts::SelectProfileResult result{};

  if (state_.recovery_state != charm::contracts::RecoveryState::kNone) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
      return result;
  }

  state_.active_profile.profile_id = request.profile_id;
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

charm::contracts::RecoveryResult DefaultSupervisor::RequestRecovery(const charm::contracts::RecoveryRequest& request) {
  charm::contracts::RecoveryResult result{};

  // Transition to kRequested, then to kRecovering, then to kNone.
  // We can reject transitions that do not make sense.

  if (request.target_state == charm::contracts::RecoveryState::kRequested) {
      if (state_.recovery_state != charm::contracts::RecoveryState::kNone) {
          result.status = charm::contracts::ContractStatus::kRejected;
          result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
          return result;
      }
      state_.recovery_state = charm::contracts::RecoveryState::kRequested;
      result.status = charm::contracts::ContractStatus::kOk;
      return result;
  }

  if (request.target_state == charm::contracts::RecoveryState::kRecovering) {
      if (state_.recovery_state != charm::contracts::RecoveryState::kRequested) {
          result.status = charm::contracts::ContractStatus::kRejected;
          result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
          return result;
      }
      state_.recovery_state = charm::contracts::RecoveryState::kRecovering;
      result.status = charm::contracts::ContractStatus::kOk;
      return result;
  }

  if (request.target_state == charm::contracts::RecoveryState::kNone) {
      if (state_.recovery_state != charm::contracts::RecoveryState::kRecovering) {
          result.status = charm::contracts::ContractStatus::kRejected;
          result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
          return result;
      }
      state_.recovery_state = charm::contracts::RecoveryState::kNone;
      result.status = charm::contracts::ContractStatus::kOk;
      return result;
  }

  result.status = charm::contracts::ContractStatus::kRejected;
  result.fault_code.category = charm::contracts::ErrorCategory::kInvalidRequest;
  return result;
}

void DefaultSupervisor::SetLastFault(const charm::contracts::FaultRecordRef& fault) {
  state_.last_fault = fault;
}

SupervisorState DefaultSupervisor::GetState() const {
  return state_;
}

}  // namespace charm::core
