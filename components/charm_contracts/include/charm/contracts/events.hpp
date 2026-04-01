#pragma once

#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/report_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::contracts {

struct LogicalGamepadState;

enum class EventSource : std::uint8_t {
  kUnknown = 0,
  kSupervisor = 1,
  kUsbHost = 2,
  kBleTransport = 3,
  kConfigStore = 4,
  kDecoder = 5,
  kMappingEngine = 6,
  kProfileManager = 7,
  kApplication = 8,
};

enum class ControlPlaneEventType : std::uint16_t {
  kUnknown = 0,
  kDeviceConnected = 1,
  kDeviceDisconnected = 2,
  kBleConnected = 3,
  kBleDisconnected = 4,
  kConfigUpdated = 5,
  kFaultRaised = 6,
  kTick = 7,
};

enum class InputElementType : std::uint8_t {
  kUnknown = 0,
  kAxis = 1,
  kButton = 2,
  kHat = 3,
  kTrigger = 4,
  kScalar = 5,
};

enum class AdapterKind : std::uint8_t {
  kUnknown = 0,
  kUsbHost = 1,
  kBleTransport = 2,
  kConfigStore = 3,
  kTime = 4,
};

enum class AdapterState : std::uint8_t {
  kUnknown = 0,
  kStopped = 1,
  kStarting = 2,
  kReady = 3,
  kRunning = 4,
  kStopping = 5,
  kFaulted = 6,
};

enum class FaultDomain : std::uint8_t {
  kUnknown = 0,
  kContract = 1,
  kAdapter = 2,
  kTransport = 3,
  kPersistence = 4,
  kConfiguration = 5,
  kRecovery = 6,
  kDeviceProtocol = 7,
};

struct ControlPlaneEvent {
  ControlPlaneEventType event_type{ControlPlaneEventType::kUnknown};
  Timestamp timestamp{};
  EventSource source{EventSource::kUnknown};
  DeviceHandle device_handle{};
  InterfaceHandle interface_handle{};
  FaultCode fault_code{};
  const void* metadata{nullptr};
  std::size_t metadata_size{0};
};

struct RawHidReportRef {
  DeviceHandle device_handle{};
  InterfaceHandle interface_handle{};
  ReportMeta report_meta{};
  std::size_t byte_length{0};
  Timestamp timestamp{};
  const std::uint8_t* bytes{nullptr};
};

struct InputElementEvent {
  ElementKeyHash element_key_hash{};
  InputElementType element_type{InputElementType::kUnknown};
  std::int32_t value{0};
  Timestamp timestamp{};
  DeviceHandle device_handle{};
  InterfaceHandle interface_handle{};
};

struct LogicalStateSnapshot {
  ProfileId profile_id{};
  Timestamp timestamp{};
  const LogicalGamepadState* state{nullptr};
};

struct FaultEvent {
  FaultDomain domain{FaultDomain::kUnknown};
  FaultCode fault_code{};
  FaultSeverity severity{FaultSeverity::kInfo};
  EventSource source{EventSource::kUnknown};
  Timestamp timestamp{};
};

struct AdapterStatusEvent {
  AdapterKind adapter_kind{AdapterKind::kUnknown};
  AdapterState state{AdapterState::kUnknown};
  Timestamp timestamp{};
};

}  // namespace charm::contracts
