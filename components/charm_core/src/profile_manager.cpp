#include "charm/core/profile_manager.hpp"

namespace charm::core {

namespace {

bool IsSupportedProfile(charm::contracts::ProfileId profile_id) {
  return profile_id.value == kGenericBleGamepadProfileId.value ||
         profile_id.value == kWirelessXboxControllerProfileId.value;
}

GetProfileCapabilitiesResult GetCapabilitiesForProfile(
    charm::contracts::ProfileId profile_id) {
  if (profile_id.value == kGenericBleGamepadProfileId.value) {
    return profile_generic_gamepad::GetCapabilities();
  }
  if (profile_id.value == kWirelessXboxControllerProfileId.value) {
    return profile_wireless_xbox_controller::GetCapabilities();
  }

  GetProfileCapabilitiesResult result{};
  result.status = charm::contracts::ContractStatus::kFailed;
  result.fault_code.category =
      charm::contracts::ErrorCategory::kUnsupportedCapability;
  return result;
}
}  // namespace

charm::contracts::SelectProfileResult CanonicalProfileManager::SelectProfile(
    const charm::contracts::SelectProfileRequest& request) {
  charm::contracts::SelectProfileResult result{};

  if (IsSupportedProfile(request.profile_id)) {
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

  if (request.profile_id.value == kGenericBleGamepadProfileId.value) {
    return profile_generic_gamepad::Encode(request);
  }
  if (request.profile_id.value == kWirelessXboxControllerProfileId.value) {
    return profile_wireless_xbox_controller::Encode(request);
  }

  result.status = charm::contracts::ContractStatus::kFailed;
  result.fault_code.category = charm::contracts::ErrorCategory::kUnsupportedCapability;
  return result;
}

GetProfileCapabilitiesResult CanonicalProfileManager::GetProfileCapabilities(
    const GetProfileCapabilitiesRequest& request) const {
  return GetCapabilitiesForProfile(request.profile_id);
}

GetSupportedProfilesResult CanonicalProfileManager::GetSupportedProfiles(
    const GetSupportedProfilesRequest& /*request*/) const {
  GetSupportedProfilesResult result{};
  result.status = charm::contracts::ContractStatus::kOk;

  const auto generic = profile_generic_gamepad::GetCapabilities();
  const auto xbox = profile_wireless_xbox_controller::GetCapabilities();

  result.descriptors[0] = generic.descriptor;
  result.descriptors[1] = xbox.descriptor;
  result.descriptor_count = 2;
  return result;
}

}  // namespace charm::core
