#pragma once

#include "charm/ports/usb_host_port.hpp"

namespace charm::test_support {

class FakeUsbHostPort : public charm::ports::UsbHostPort {
 public:
  void SetStartResult(charm::contracts::StartResult result) { start_result_ = result; }
  void SetStopResult(charm::contracts::StopResult result) { stop_result_ = result; }
  void SetClaimInterfaceResult(charm::ports::ClaimInterfaceResult result) { claim_result_ = result; }

  void EmitDeviceConnected(const charm::ports::UsbEnumerationInfo& info,
                           const charm::ports::DeviceDescriptorRef& descriptor) const {
    if (listener_ != nullptr) {
      listener_->OnDeviceConnected(info, descriptor);
    }
  }

  void EmitDeviceDisconnected(charm::contracts::DeviceHandle device_handle) const {
    if (listener_ != nullptr) {
      listener_->OnDeviceDisconnected(device_handle);
    }
  }

  void EmitInterfaceDescriptor(const charm::ports::InterfaceDescriptorRef& descriptor) const {
    if (listener_ != nullptr) {
      listener_->OnInterfaceDescriptorAvailable(descriptor);
    }
  }

  void EmitReport(const charm::contracts::RawHidReportRef& report_ref) const {
    if (listener_ != nullptr) {
      listener_->OnReportReceived(report_ref);
    }
  }

  void EmitStatus(const charm::ports::UsbHostStatus& status) const {
    if (listener_ != nullptr) {
      listener_->OnStatusChanged(status);
    }
  }

  charm::contracts::StartResult Start(const charm::contracts::StartRequest&) override { return start_result_; }
  charm::contracts::StopResult Stop(const charm::contracts::StopRequest&) override { return stop_result_; }

  charm::ports::ClaimInterfaceResult ClaimInterface(const charm::ports::ClaimInterfaceRequest&) override {
    return claim_result_;
  }

  void SetListener(charm::ports::UsbHostPortListener* listener) override { listener_ = listener; }

 private:
  charm::contracts::StartResult start_result_{};
  charm::contracts::StopResult stop_result_{};
  charm::ports::ClaimInterfaceResult claim_result_{};
  charm::ports::UsbHostPortListener* listener_{nullptr};
};

}  // namespace charm::test_support
