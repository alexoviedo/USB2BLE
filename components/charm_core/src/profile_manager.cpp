#include "charm/core/profile_manager.hpp"

namespace charm::core {

namespace {
constexpr charm::contracts::ProfileId kGenericGamepadProfileId{1};
}  // namespace

charm::contracts::SelectProfileResult CanonicalProfileManager::SelectProfile(
    const charm::contracts::SelectProfileRequest& request) {
  charm::contracts::SelectProfileResult result{};

  if (request.profile_id.value == kGenericGamepadProfileId.value) {
    selected_profile_ = request.profile_id;
    result.status = charm::contracts::ContractStatus::kOk;
  } else {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code.category = charm::contracts::ErrorCategory::kUnsupportedCapability;
  }

  return result;
}

EncodeLogicalStateResult CanonicalProfileManager::EncodeLogicalState(
    const EncodeLogicalStateRequest& request) const {
  EncodeLogicalStateResult result{};

  if (request.profile_id.value != selected_profile_.value) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
    return result;
  }

  if (request.profile_id.value == kGenericGamepadProfileId.value) {
    return profile_generic_gamepad::Encode(request);
  }

  result.status = charm::contracts::ContractStatus::kFailed;
  result.fault_code.category = charm::contracts::ErrorCategory::kUnsupportedCapability;
  return result;
}

GetProfileCapabilitiesResult CanonicalProfileManager::GetProfileCapabilities(
    const GetProfileCapabilitiesRequest& request) const {
  GetProfileCapabilitiesResult result{};

  if (request.profile_id.value == kGenericGamepadProfileId.value) {
    return profile_generic_gamepad::GetCapabilities();
  }

  result.status = charm::contracts::ContractStatus::kFailed;
  result.fault_code.category = charm::contracts::ErrorCategory::kUnsupportedCapability;
  return result;
}

}  // namespace charm::core
