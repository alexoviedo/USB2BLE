#include <cassert>

#include "charm/core/device_registry.hpp"

int main() {
  charm::core::InMemoryDeviceRegistry registry;

  charm::core::RegisterDeviceRequest register_device{};
  register_device.enumeration_info.vendor_id = 0x1234;
  register_device.enumeration_info.product_id = 0x5678;

  const auto device_result = registry.RegisterDevice(register_device);
  assert(device_result.status == charm::contracts::ContractStatus::kOk);
  assert(device_result.device_handle.value != 0);

  const auto lookup_ok = registry.LookupDevice(charm::core::LookupDeviceRequest{.device_handle = device_result.device_handle});
  assert(lookup_ok.status == charm::contracts::ContractStatus::kOk);
  assert(lookup_ok.enumeration_info.vendor_id == 0x1234);

  charm::core::RegisterInterfaceRequest register_interface{};
  register_interface.interface_descriptor.interface_handle.value = 42;
  register_interface.interface_descriptor.interface_number = 1;

  const auto interface_result = registry.RegisterInterface(register_interface);
  assert(interface_result.status == charm::contracts::ContractStatus::kOk);

  charm::contracts::DecodePlanRef decode_plan{.plan = reinterpret_cast<const void*>(0x1)};
  const auto attach_result = registry.AttachDecodePlan(
      charm::core::AttachDecodePlanRequest{.interface_handle = register_interface.interface_descriptor.interface_handle,
                                           .decode_plan = decode_plan});
  assert(attach_result.status == charm::contracts::ContractStatus::kOk);
  assert(attach_result.registry_entry.decode_plan.plan == decode_plan.plan);

  const auto detach_result = registry.DetachDevice(charm::core::DetachDeviceRequest{.device_handle = device_result.device_handle});
  assert(detach_result.status == charm::contracts::ContractStatus::kOk);

  const auto lookup_missing = registry.LookupDevice(charm::core::LookupDeviceRequest{.device_handle = device_result.device_handle});
  assert(lookup_missing.status == charm::contracts::ContractStatus::kRejected);

  return 0;
}
