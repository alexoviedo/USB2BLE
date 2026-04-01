#pragma once

#include "charm/ports/ble_transport_port.hpp"

namespace charm::test_support {

class FakeBleTransportPort : public charm::ports::BleTransportPort {
 public:
  void SetStartResult(charm::contracts::StartResult result) { start_result_ = result; }
  void SetStopResult(charm::contracts::StopResult result) { stop_result_ = result; }
  void SetNotifyResult(charm::ports::NotifyInputReportResult result) { notify_result_ = result; }

  void EmitPeerConnected(const charm::ports::BlePeerInfo& peer_info) const {
    if (listener_ != nullptr) {
      listener_->OnPeerConnected(peer_info);
    }
  }

  void EmitPeerDisconnected(const charm::ports::BlePeerInfo& peer_info) const {
    if (listener_ != nullptr) {
      listener_->OnPeerDisconnected(peer_info);
    }
  }

  void EmitStatus(const charm::ports::BleTransportStatus& status) const {
    if (listener_ != nullptr) {
      listener_->OnStatusChanged(status);
    }
  }

  charm::contracts::StartResult Start(const charm::contracts::StartRequest&) override { return start_result_; }
  charm::contracts::StopResult Stop(const charm::contracts::StopRequest&) override { return stop_result_; }

  charm::ports::NotifyInputReportResult NotifyInputReport(const charm::ports::NotifyInputReportRequest&) override {
    return notify_result_;
  }

  void SetListener(charm::ports::BleTransportPortListener* listener) override { listener_ = listener; }

 private:
  charm::contracts::StartResult start_result_{};
  charm::contracts::StopResult stop_result_{};
  charm::ports::NotifyInputReportResult notify_result_{};
  charm::ports::BleTransportPortListener* listener_{nullptr};
};

}  // namespace charm::test_support
