#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/contracts/transport_types.hpp"
#include "charm/core/logical_state.hpp"

namespace charm::core {

inline constexpr std::size_t kMaxProfileNameLength = 32;
inline constexpr std::size_t kMaxSupportedProfiles = 4;
inline constexpr charm::contracts::ProfileId kGenericBleGamepadProfileId{1};
inline constexpr charm::contracts::ProfileId kWirelessXboxControllerProfileId{2};

enum class ProfileCapability : std::uint8_t {
  kUnknown = 0,
  kSupportsHat = 1,
  kSupportsAnalogTriggers = 2,
  kMapsHatToDpadButtons = 3,
  kSupportsHighResolutionAxes = 4,
};

struct ProfileDescriptor {
  charm::contracts::ProfileId profile_id{};
  const char* name{""};
  std::size_t name_length{0};
  charm::contracts::ReportId report_id{0};
  std::size_t report_size{0};
  const ProfileCapability* capabilities{nullptr};
  std::size_t capability_count{0};
};

struct EncodeLogicalStateRequest {
  charm::contracts::ProfileId profile_id{};
  const charm::contracts::LogicalGamepadState* logical_state{nullptr};
  std::uint8_t* output_buffer{nullptr};
  std::size_t output_buffer_capacity{0};
};

struct EncodeLogicalStateResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::EncodedInputReport report{};
};

struct GetProfileCapabilitiesRequest {
  charm::contracts::ProfileId profile_id{};
};

struct GetProfileCapabilitiesResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  ProfileDescriptor descriptor{};
};

struct GetSupportedProfilesRequest {};

struct GetSupportedProfilesResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  std::array<ProfileDescriptor, kMaxSupportedProfiles> descriptors{};
  std::size_t descriptor_count{0};
};

class ProfileManager {
 public:
  virtual ~ProfileManager() = default;

  virtual charm::contracts::SelectProfileResult SelectProfile(const charm::contracts::SelectProfileRequest& request) = 0;
  virtual EncodeLogicalStateResult EncodeLogicalState(const EncodeLogicalStateRequest& request) const = 0;
  virtual GetProfileCapabilitiesResult GetProfileCapabilities(const GetProfileCapabilitiesRequest& request) const = 0;
  virtual GetSupportedProfilesResult GetSupportedProfiles(const GetSupportedProfilesRequest& request) const = 0;
};

class CanonicalProfileManager final : public ProfileManager {
 public:
  CanonicalProfileManager() = default;
  ~CanonicalProfileManager() override = default;

  charm::contracts::SelectProfileResult SelectProfile(const charm::contracts::SelectProfileRequest& request) override;
  EncodeLogicalStateResult EncodeLogicalState(const EncodeLogicalStateRequest& request) const override;
  GetProfileCapabilitiesResult GetProfileCapabilities(const GetProfileCapabilitiesRequest& request) const override;
  GetSupportedProfilesResult GetSupportedProfiles(const GetSupportedProfilesRequest& request) const override;

 private:
  charm::contracts::ProfileId selected_profile_{};
};

// Expose generic gamepad encoding for the manager to use
namespace profile_generic_gamepad {
  EncodeLogicalStateResult Encode(const EncodeLogicalStateRequest& request);
  GetProfileCapabilitiesResult GetCapabilities();
}

namespace profile_wireless_xbox_controller {
  EncodeLogicalStateResult Encode(const EncodeLogicalStateRequest& request);
  GetProfileCapabilitiesResult GetCapabilities();
}

}  // namespace charm::core
