#include "charm/core/profile_manager.hpp"

#include <algorithm>
#include <cstdint>

namespace charm::core::profile_wireless_xbox_controller {

namespace {

constexpr charm::contracts::ReportId kInputReportId{2};

struct __attribute__((packed)) WirelessXboxControllerReport {
  std::uint16_t buttons{0};
  std::uint8_t dpad{0};
  std::uint8_t left_trigger{0};
  std::uint8_t right_trigger{0};
  std::int16_t left_x{0};
  std::int16_t left_y{0};
  std::int16_t right_x{0};
  std::int16_t right_y{0};
};

const ProfileCapability kCapabilities[] = {
    ProfileCapability::kSupportsAnalogTriggers,
    ProfileCapability::kMapsHatToDpadButtons,
    ProfileCapability::kSupportsHighResolutionAxes,
};

constexpr const char* kProfileName = "Wireless Xbox Controller";

constexpr std::uint8_t kDpadUpBit = 1u << 0;
constexpr std::uint8_t kDpadDownBit = 1u << 1;
constexpr std::uint8_t kDpadLeftBit = 1u << 2;
constexpr std::uint8_t kDpadRightBit = 1u << 3;

std::uint16_t EncodeButtons(const charm::contracts::LogicalGamepadState& state) {
  std::uint16_t buttons = 0;
  for (std::size_t i = 0; i < 16; ++i) {
    if (state.buttons[i].pressed) {
      buttons |= static_cast<std::uint16_t>(1u << i);
    }
  }
  return buttons;
}

std::uint8_t EncodeDpad(std::uint8_t hat_value) {
  switch (hat_value) {
    case 0:
      return kDpadUpBit;
    case 1:
      return kDpadUpBit | kDpadRightBit;
    case 2:
      return kDpadRightBit;
    case 3:
      return kDpadDownBit | kDpadRightBit;
    case 4:
      return kDpadDownBit;
    case 5:
      return kDpadDownBit | kDpadLeftBit;
    case 6:
      return kDpadLeftBit;
    case 7:
      return kDpadUpBit | kDpadLeftBit;
    default:
      return 0;
  }
}

std::int16_t ExpandAxis(std::int32_t logical_value) {
  const auto clamped = std::clamp<std::int32_t>(logical_value, -128, 127);
  const auto expanded =
      std::clamp<std::int64_t>(static_cast<std::int64_t>(clamped) * 256ll,
                               -32768ll, 32512ll);
  return static_cast<std::int16_t>(expanded);
}

std::uint8_t ClampTrigger(std::uint16_t logical_value) {
  return static_cast<std::uint8_t>(std::min<std::uint16_t>(logical_value, 255));
}

}  // namespace

GetProfileCapabilitiesResult GetCapabilities() {
  GetProfileCapabilitiesResult result{};
  result.status = charm::contracts::ContractStatus::kOk;
  result.descriptor.profile_id = kWirelessXboxControllerProfileId;
  result.descriptor.name = kProfileName;
  std::size_t name_length = 0;
  while (kProfileName[name_length] != '\0') {
    ++name_length;
  }
  result.descriptor.name_length = name_length;
  result.descriptor.report_id = kInputReportId;
  result.descriptor.report_size = sizeof(WirelessXboxControllerReport);
  result.descriptor.capabilities = kCapabilities;
  result.descriptor.capability_count = sizeof(kCapabilities) / sizeof(kCapabilities[0]);
  return result;
}

EncodeLogicalStateResult Encode(const EncodeLogicalStateRequest& request) {
  EncodeLogicalStateResult result{};

  if (request.logical_state == nullptr || request.output_buffer == nullptr ||
      request.output_buffer_capacity < sizeof(WirelessXboxControllerReport)) {
    result.status = charm::contracts::ContractStatus::kFailed;
    result.fault_code.category = charm::contracts::ErrorCategory::kInvalidRequest;
    return result;
  }

  WirelessXboxControllerReport* report_ptr =
      new (request.output_buffer) WirelessXboxControllerReport();
  report_ptr->buttons = EncodeButtons(*request.logical_state);
  report_ptr->dpad = EncodeDpad(request.logical_state->hat.value);
  report_ptr->left_trigger = ClampTrigger(request.logical_state->left_trigger.value);
  report_ptr->right_trigger = ClampTrigger(request.logical_state->right_trigger.value);
  report_ptr->left_x = ExpandAxis(request.logical_state->axes[0].value);
  report_ptr->left_y = ExpandAxis(request.logical_state->axes[1].value);
  report_ptr->right_x = ExpandAxis(request.logical_state->axes[2].value);
  report_ptr->right_y = ExpandAxis(request.logical_state->axes[3].value);

  result.status = charm::contracts::ContractStatus::kOk;
  result.report.report_id = kInputReportId;
  result.report.bytes = request.output_buffer;
  result.report.size = sizeof(WirelessXboxControllerReport);
  return result;
}

}  // namespace charm::core::profile_wireless_xbox_controller
