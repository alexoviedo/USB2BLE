#include "charm/core/logical_state.hpp"

namespace charm::core {

CanonicalLogicalStateStore::CanonicalLogicalStateStore(charm::contracts::ProfileId profile_id)
    : profile_id_{profile_id} {}

GetLogicalStateResult CanonicalLogicalStateStore::GetLogicalState(
    const GetLogicalStateRequest& request) const {
  GetLogicalStateResult result{};
  if (request.profile_id.value != profile_id_.value) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        charm::contracts::ErrorCategory::kContractViolation, 0};
    return result;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  result.snapshot.profile_id = profile_id_;
  result.snapshot.timestamp = last_update_time_;
  result.snapshot.state = &state_;
  return result;
}

ResetLogicalStateResult CanonicalLogicalStateStore::ResetLogicalState(
    const ResetLogicalStateRequest& /*request*/) {
  state_ = charm::contracts::LogicalGamepadState{};
  last_update_time_ = charm::contracts::Timestamp{0};
  ResetLogicalStateResult result{};
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

charm::contracts::LogicalGamepadState& CanonicalLogicalStateStore::GetMutableState(
    charm::contracts::Timestamp current_time) {
  last_update_time_ = current_time;
  return state_;
}

}  // namespace charm::core
