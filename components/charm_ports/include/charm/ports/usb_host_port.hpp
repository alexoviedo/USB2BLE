#pragma once

#include <cstdint>

#include "charm/contracts/events.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/transport_types.hpp"

namespace charm::ports {

struct UsbHostStatus {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::AdapterState state{charm::contracts::AdapterState::kUnknown};
};

struct UsbEnumerationInfo {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::VendorId vendor_id{0};
  charm::contracts::ProductId product_id{0};
  charm::contracts::HubPath hub_path{};
};

struct DeviceDescriptorRef {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::RawDescriptorRef descriptor{};
};

struct InterfaceDescriptorRef {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::InterfaceHandle interface_handle{};
  charm::contracts::InterfaceNumber interface_number{0};
  charm::contracts::RawDescriptorRef descriptor{};
};

struct ClaimInterfaceRequest {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::InterfaceNumber interface_number{0};
};

struct ClaimInterfaceResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::InterfaceHandle interface_handle{};
};

class UsbHostPortListener {
 public:
  virtual ~UsbHostPortListener() = default;

  virtual void OnDeviceConnected(const UsbEnumerationInfo& enumeration_info,
                                 const DeviceDescriptorRef& device_descriptor) = 0;
  virtual void OnDeviceDisconnected(charm::contracts::DeviceHandle device_handle) = 0;
  virtual void OnInterfaceDescriptorAvailable(const InterfaceDescriptorRef& interface_descriptor) = 0;
  virtual void OnReportReceived(const charm::contracts::RawHidReportRef& report_ref) = 0;
  virtual void OnStatusChanged(const UsbHostStatus& status) = 0;
};

class UsbHostPort {
 public:
  virtual ~UsbHostPort() = default;

  virtual charm::contracts::StartResult Start(const charm::contracts::StartRequest& request) = 0;
  virtual charm::contracts::StopResult Stop(const charm::contracts::StopRequest& request) = 0;
  virtual ClaimInterfaceResult ClaimInterface(const ClaimInterfaceRequest& request) = 0;
  virtual void SetListener(UsbHostPortListener* listener) = 0;
};

}  // namespace charm::ports
