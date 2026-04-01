#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/events.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/transport_types.hpp"

namespace charm::ports {

struct BleTransportStatus {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::AdapterState state{charm::contracts::AdapterState::kUnknown};
};

struct BlePeerInfo {
  std::array<std::uint8_t, 6> address{};
  bool bonded{false};
};

struct BondingMaterialRef {
  const std::uint8_t* bytes{nullptr};
  std::size_t size{0};
};

struct NotifyInputReportRequest {
  charm::contracts::EncodedInputReport report{};
};

struct NotifyInputReportResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

class BleTransportPortListener {
 public:
  virtual ~BleTransportPortListener() = default;

  virtual void OnPeerConnected(const BlePeerInfo& peer_info) = 0;
  virtual void OnPeerDisconnected(const BlePeerInfo& peer_info) = 0;
  virtual void OnStatusChanged(const BleTransportStatus& status) = 0;
};

class BleTransportPort {
 public:
  virtual ~BleTransportPort() = default;

  virtual charm::contracts::StartResult Start(const charm::contracts::StartRequest& request) = 0;
  virtual charm::contracts::StopResult Stop(const charm::contracts::StopRequest& request) = 0;
  virtual NotifyInputReportResult NotifyInputReport(const NotifyInputReportRequest& request) = 0;
  virtual void SetListener(BleTransportPortListener* listener) = 0;
};

}  // namespace charm::ports
