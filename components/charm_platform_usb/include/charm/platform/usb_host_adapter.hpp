#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "charm/ports/usb_host_port.hpp"

#if __has_include("usb/hid_host.h") && __has_include("usb/usb_host.h")
#define CHARM_USB_HOST_ESP_IDF_AVAILABLE 1
#include "usb/hid_host.h"
#include "usb/usb_host.h"
struct hid_interface;
#else
#define CHARM_USB_HOST_ESP_IDF_AVAILABLE 0
using hid_host_device_handle_t = void*;
#endif

namespace charm::platform {

class UsbHostAdapter : public charm::ports::UsbHostPort {
 public:
  UsbHostAdapter();
  ~UsbHostAdapter() override;

  charm::contracts::StartResult Start(const charm::contracts::StartRequest& request) override;
  charm::contracts::StopResult Stop(const charm::contracts::StopRequest& request) override;
  charm::ports::ClaimInterfaceResult ClaimInterface(const charm::ports::ClaimInterfaceRequest& request) override;
  void SetListener(charm::ports::UsbHostPortListener* listener) override;

  // These methods simulate the callbacks from the ESP-IDF USB Host driver
  // Enqueue facts only, testing normalized dispatch.
  void SimulateDeviceConnected(const charm::ports::UsbEnumerationInfo& info, const charm::ports::DeviceDescriptorRef& desc);
  void SimulateDeviceDisconnected(charm::contracts::DeviceHandle device_handle);
  void SimulateInterfaceDescriptorAvailable(const charm::ports::InterfaceDescriptorRef& desc);
  void SimulateReportReceived(const charm::contracts::RawHidReportRef& report_ref);
  void SimulateStatusChanged(const charm::ports::UsbHostStatus& status);

 private:
  struct HidInterfaceContext {
    charm::contracts::DeviceHandle device_handle{};
    charm::contracts::InterfaceHandle interface_handle{};
    charm::contracts::InterfaceNumber interface_number{0};
    std::uint8_t device_address{0};
    hid_host_device_handle_t hid_handle{nullptr};
    std::vector<std::uint8_t> report_descriptor{};
  };

  struct DeviceContext {
    charm::contracts::DeviceHandle device_handle{};
    std::uint8_t device_address{0};
    charm::contracts::VendorId vendor_id{0};
    charm::contracts::ProductId product_id{0};
    std::vector<std::uint8_t> device_descriptor{};
    charm::contracts::HubPath hub_path{};
    std::unordered_map<std::uint8_t, charm::contracts::InterfaceHandle> claimed_interfaces{};
  };

  static void OnHidDriverEvent(hid_host_device_handle_t hid_device_handle,
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
                               hid_host_driver_event_t event,
#else
                               int event,
#endif
                               void* arg);
  static void OnHidInterfaceEvent(hid_host_device_handle_t hid_device_handle,
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
                                  hid_host_interface_event_t event,
#else
                                  int event,
#endif
                                  void* arg);

  void HandleHidDriverConnected(hid_host_device_handle_t hid_device_handle);
  void HandleHidInputReport(hid_host_device_handle_t hid_device_handle);
  void HandleHidTransferError(hid_host_device_handle_t hid_device_handle);
  void HandleHidDisconnected(hid_host_device_handle_t hid_device_handle);

  void EmitStatusLocked(charm::contracts::ContractStatus status,
                        charm::contracts::AdapterState state,
                        charm::contracts::ErrorCategory category,
                        std::uint32_t reason);
  charm::contracts::InterfaceHandle ResolveInterfaceHandleLocked(
      const charm::contracts::DeviceHandle& device_handle,
      charm::contracts::InterfaceNumber interface_number);

  bool InstallHostStack();
  void UninstallHostStack();
  void ShutdownInterfacesLocked();

  std::mutex mutex_;
  charm::ports::UsbHostPortListener* listener_{nullptr};
  bool started_{false};
  uint32_t next_interface_handle_id_{1};
  uint32_t next_device_handle_id_{1};

  std::unordered_map<std::uint8_t, DeviceContext> devices_by_address_{};
  std::unordered_map<std::uintptr_t, HidInterfaceContext> interfaces_by_handle_{};
  std::unordered_map<std::uint32_t, std::uint8_t> device_handle_to_address_{};

  std::thread usb_lib_thread_{};
  std::atomic<bool> usb_lib_thread_running_{false};
};

}  // namespace charm::platform
