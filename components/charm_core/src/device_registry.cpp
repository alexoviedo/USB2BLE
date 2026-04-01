#include "charm/core/device_registry.hpp"

namespace charm::core {
namespace {

charm::contracts::FaultCode MakeFault(charm::contracts::ErrorCategory category, std::uint32_t reason) {
  return charm::contracts::FaultCode{.category = category, .reason = reason};
}

}  // namespace

InMemoryDeviceRegistry::InMemoryDeviceRegistry(std::size_t max_devices, std::size_t max_interfaces)
    : max_devices_(max_devices), max_interfaces_(max_interfaces) {}

RegisterDeviceResult InMemoryDeviceRegistry::RegisterDevice(const RegisterDeviceRequest& request) {
  if (devices_.size() >= max_devices_) {
    return RegisterDeviceResult{.status = charm::contracts::ContractStatus::kRejected,
                                .fault_code = MakeFault(charm::contracts::ErrorCategory::kCapacityExceeded, 1),
                                .device_handle = {0}};
  }

  charm::contracts::DeviceHandle handle = request.enumeration_info.device_handle;
  if (handle.value == 0) {
    handle.value = next_device_handle_++;
  }

  if (devices_.find(handle.value) != devices_.end()) {
    return RegisterDeviceResult{.status = charm::contracts::ContractStatus::kRejected,
                                .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidState, 2),
                                .device_handle = handle};
  }

  auto info = request.enumeration_info;
  info.device_handle = handle;
  devices_.emplace(handle.value, info);

  return RegisterDeviceResult{.status = charm::contracts::ContractStatus::kOk, .fault_code = {}, .device_handle = handle};
}

RegisterInterfaceResult InMemoryDeviceRegistry::RegisterInterface(const RegisterInterfaceRequest& request) {
  if (interfaces_.size() >= max_interfaces_) {
    return RegisterInterfaceResult{.status = charm::contracts::ContractStatus::kRejected,
                                   .fault_code = MakeFault(charm::contracts::ErrorCategory::kCapacityExceeded, 3),
                                   .registry_entry = {}};
  }

  const auto interface_handle = request.interface_descriptor.interface_handle;
  if (interface_handle.value == 0) {
    return RegisterInterfaceResult{.status = charm::contracts::ContractStatus::kRejected,
                                   .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidRequest, 4),
                                   .registry_entry = {}};
  }

  if (interfaces_.find(interface_handle.value) != interfaces_.end()) {
    return RegisterInterfaceResult{.status = charm::contracts::ContractStatus::kRejected,
                                   .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidState, 5),
                                   .registry_entry = {}};
  }

  charm::contracts::RegistryEntry entry{};
  entry.device_handle = request.interface_descriptor.device_handle;
  entry.interface_handle = interface_handle;
  entry.interface_number = request.interface_descriptor.interface_number;
  interfaces_.emplace(interface_handle.value, entry);

  return RegisterInterfaceResult{.status = charm::contracts::ContractStatus::kOk, .fault_code = {}, .registry_entry = entry};
}

DetachDeviceResult InMemoryDeviceRegistry::DetachDevice(const DetachDeviceRequest& request) {
  const auto erased = devices_.erase(request.device_handle.value);
  if (erased == 0) {
    return DetachDeviceResult{.status = charm::contracts::ContractStatus::kRejected,
                              .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidRequest, 6)};
  }

  for (auto it = interfaces_.begin(); it != interfaces_.end();) {
    if (it->second.device_handle.value == request.device_handle.value) {
      it = interfaces_.erase(it);
    } else {
      ++it;
    }
  }

  return DetachDeviceResult{.status = charm::contracts::ContractStatus::kOk, .fault_code = {}};
}

LookupDeviceResult InMemoryDeviceRegistry::LookupDevice(const LookupDeviceRequest& request) const {
  const auto it = devices_.find(request.device_handle.value);
  if (it == devices_.end()) {
    return LookupDeviceResult{.status = charm::contracts::ContractStatus::kRejected,
                              .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidRequest, 7),
                              .enumeration_info = {}};
  }

  return LookupDeviceResult{.status = charm::contracts::ContractStatus::kOk, .fault_code = {}, .enumeration_info = it->second};
}

AttachDecodePlanResult InMemoryDeviceRegistry::AttachDecodePlan(const AttachDecodePlanRequest& request) {
  const auto it = interfaces_.find(request.interface_handle.value);
  if (it == interfaces_.end()) {
    return AttachDecodePlanResult{.status = charm::contracts::ContractStatus::kRejected,
                                  .fault_code = MakeFault(charm::contracts::ErrorCategory::kInvalidRequest, 8),
                                  .registry_entry = {}};
  }

  it->second.decode_plan = request.decode_plan;

  return AttachDecodePlanResult{.status = charm::contracts::ContractStatus::kOk, .fault_code = {}, .registry_entry = it->second};
}

}  // namespace charm::core
