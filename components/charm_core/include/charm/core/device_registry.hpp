#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "charm/contracts/registry_types.hpp"
#include "charm/ports/usb_host_port.hpp"

namespace charm::core {

struct RegisterDeviceRequest {
  charm::ports::UsbEnumerationInfo enumeration_info{};
  charm::ports::DeviceDescriptorRef device_descriptor{};
};

struct RegisterDeviceResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::DeviceHandle device_handle{};
};

struct RegisterInterfaceRequest {
  charm::ports::InterfaceDescriptorRef interface_descriptor{};
};

struct RegisterInterfaceResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::RegistryEntry registry_entry{};
};

struct DetachDeviceRequest {
  charm::contracts::DeviceHandle device_handle{};
};

struct DetachDeviceResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

struct LookupDeviceRequest {
  charm::contracts::DeviceHandle device_handle{};
};

struct LookupDeviceResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::ports::UsbEnumerationInfo enumeration_info{};
};

struct AttachDecodePlanRequest {
  charm::contracts::InterfaceHandle interface_handle{};
  charm::contracts::DecodePlanRef decode_plan{};
};

struct AttachDecodePlanResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::RegistryEntry registry_entry{};
};

class DeviceRegistry {
 public:
  virtual ~DeviceRegistry() = default;

  virtual RegisterDeviceResult RegisterDevice(const RegisterDeviceRequest& request) = 0;
  virtual RegisterInterfaceResult RegisterInterface(const RegisterInterfaceRequest& request) = 0;
  virtual DetachDeviceResult DetachDevice(const DetachDeviceRequest& request) = 0;
  virtual LookupDeviceResult LookupDevice(const LookupDeviceRequest& request) const = 0;
  virtual AttachDecodePlanResult AttachDecodePlan(const AttachDecodePlanRequest& request) = 0;
};

inline constexpr std::size_t kDefaultMaxRegisteredDevices = 16;
inline constexpr std::size_t kDefaultMaxRegisteredInterfaces = 32;

class InMemoryDeviceRegistry final : public DeviceRegistry {
 public:
  explicit InMemoryDeviceRegistry(std::size_t max_devices = kDefaultMaxRegisteredDevices,
                                  std::size_t max_interfaces = kDefaultMaxRegisteredInterfaces);

  RegisterDeviceResult RegisterDevice(const RegisterDeviceRequest& request) override;
  RegisterInterfaceResult RegisterInterface(const RegisterInterfaceRequest& request) override;
  DetachDeviceResult DetachDevice(const DetachDeviceRequest& request) override;
  LookupDeviceResult LookupDevice(const LookupDeviceRequest& request) const override;
  AttachDecodePlanResult AttachDecodePlan(const AttachDecodePlanRequest& request) override;

 private:
  std::size_t max_devices_{0};
  std::size_t max_interfaces_{0};
  std::uint32_t next_device_handle_{1};
  std::unordered_map<std::uint32_t, charm::ports::UsbEnumerationInfo> devices_{};
  std::unordered_map<std::uint32_t, charm::contracts::RegistryEntry> interfaces_{};
};

}  // namespace charm::core
