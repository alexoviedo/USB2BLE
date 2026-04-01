#include <gtest/gtest.h>
#include <memory>
#include "charm/platform/ble_transport_adapter.hpp"

namespace charm::platform::test {

class FakeBleLifecycleBackend final : public charm::platform::BleLifecycleBackend {
 public:
  bool RegisterStackEventSink(StackEventSink* sink) override {
    sink_ = sink;
    register_calls++;
    return register_result;
  }

  bool UsesStackEventCallbacks() const override {
    return use_stack_callbacks;
  }

  bool Start() override {
    start_calls++;
    return start_result;
  }

  bool Stop() override {
    stop_calls++;
    return stop_result;
  }

  bool ConfigureReportChannel(std::uint32_t transport_if, std::uint16_t connection_id,
                              std::uint16_t value_handle, bool require_confirmation) override {
    configure_calls++;
    last_transport_if = transport_if;
    last_connection_id = connection_id;
    last_value_handle = value_handle;
    last_require_confirmation = require_confirmation;
    report_channel_ready = true;
    return configure_result;
  }

  void ClearReportChannel() override {
    clear_calls++;
    report_channel_ready = false;
  }

  bool SendReport(const charm::contracts::EncodedInputReport& report) override {
    send_calls++;
    last_report_id = report.report_id;
    last_report_size = report.size;
    return send_result && report_channel_ready;
  }

  bool start_result{true};
  bool stop_result{true};
  bool register_result{true};
  bool use_stack_callbacks{false};
  bool configure_result{true};
  bool send_result{true};
  bool report_channel_ready{false};
  int register_calls{0};
  int start_calls{0};
  int stop_calls{0};
  int configure_calls{0};
  int clear_calls{0};
  int send_calls{0};
  charm::contracts::ReportId last_report_id{0};
  std::size_t last_report_size{0};
  std::uint32_t last_transport_if{0};
  std::uint16_t last_connection_id{0};
  std::uint16_t last_value_handle{0};
  bool last_require_confirmation{false};

  void EmitAdvertisingReady() {
    if (sink_ != nullptr) {
      sink_->OnStackAdvertisingReady();
    }
  }

  void EmitPeerConnected(const charm::ports::BlePeerInfo& peer) {
    if (sink_ != nullptr) {
      sink_->OnStackPeerConnected(peer);
    }
  }

  void EmitPeerDisconnected(const charm::ports::BlePeerInfo& peer) {
    if (sink_ != nullptr) {
      sink_->OnStackPeerDisconnected(peer);
    }
  }

  void EmitReportChannelReady(std::uint32_t transport_if, std::uint16_t connection_id,
                              std::uint16_t value_handle, bool require_confirmation) {
    if (sink_ != nullptr) {
      sink_->OnStackReportChannelReady(transport_if, connection_id, value_handle,
                                       require_confirmation);
    }
  }

  void EmitReportChannelClosed() {
    if (sink_ != nullptr) {
      sink_->OnStackReportChannelClosed();
    }
  }

  void EmitLifecycleError(std::uint32_t reason) {
    if (sink_ != nullptr) {
      sink_->OnStackLifecycleError(reason);
    }
  }

 private:
  StackEventSink* sink_{nullptr};
};

class MockBleTransportPortListener : public charm::ports::BleTransportPortListener {
 public:
  void OnPeerConnected(const charm::ports::BlePeerInfo& peer_info) override {
    connected_count++;
    last_peer = peer_info;
  }

  void OnPeerDisconnected(const charm::ports::BlePeerInfo& peer_info) override {
    disconnected_count++;
    last_peer = peer_info;
  }

  void OnStatusChanged(const charm::ports::BleTransportStatus& status) override {
    status_changed_count++;
    last_status = status;
  }

  int connected_count{0};
  int disconnected_count{0};
  int status_changed_count{0};
  charm::ports::BlePeerInfo last_peer{};
  charm::ports::BleTransportStatus last_status{};
};

class BleTransportAdapterTest : public ::testing::Test {
 protected:
  std::unique_ptr<FakeBleLifecycleBackend> backend{std::make_unique<FakeBleLifecycleBackend>()};
  FakeBleLifecycleBackend* backend_raw{backend.get()};
  BleTransportAdapter adapter{std::move(backend)};
  MockBleTransportPortListener listener;

  void SetUp() override {
    adapter.SetListener(&listener);
  }
};

TEST_F(BleTransportAdapterTest, StartSucceedsAndNotifiesListener) {
  charm::contracts::StartRequest req;
  auto result = adapter.Start(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(backend_raw->register_calls, 1);
  EXPECT_EQ(backend_raw->start_calls, 1);

  EXPECT_EQ(listener.status_changed_count, 1);
  EXPECT_EQ(listener.last_status.state, charm::contracts::AdapterState::kRunning);
}

TEST_F(BleTransportAdapterTest, StopSucceedsAndNotifiesListener) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);
  listener.status_changed_count = 0; // reset

  charm::contracts::StopRequest req;
  auto result = adapter.Stop(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(backend_raw->stop_calls, 1);

  EXPECT_EQ(listener.status_changed_count, 1);
  EXPECT_EQ(listener.last_status.state, charm::contracts::AdapterState::kStopped);
}

TEST_F(BleTransportAdapterTest, NotifyInputReportIsUnavailableWithoutPeerAndReportChannel) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 1;
  req.report.size = 10;

  auto result = adapter.NotifyInputReport(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kUnavailable);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kInvalidState);
}

TEST_F(BleTransportAdapterTest, StartWhenAlreadyRunningIsRejected) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  auto result = adapter.Start(start_req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(BleTransportAdapterTest, StopWhenAlreadyStoppedIsRejected) {
  charm::contracts::StopRequest stop_req;
  auto result = adapter.Stop(stop_req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(BleTransportAdapterTest, NotifyWhenStoppedIsRejected) {
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 1;
  req.report.size = 10;

  auto result = adapter.NotifyInputReport(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(BleTransportAdapterTest, StartFailureSurfacesAdapterFailureStatus) {
  backend_raw->register_result = false;
  charm::contracts::StartRequest req;
  auto result = adapter.Start(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kAdapterFailure);
  EXPECT_EQ(listener.last_status.status, charm::contracts::ContractStatus::kFailed);
}

TEST_F(BleTransportAdapterTest, StackCallbackModeWaitsForAdvertisingReadyEvent) {
  backend_raw->use_stack_callbacks = true;

  charm::contracts::StartRequest req;
  const auto start_result = adapter.Start(req);
  EXPECT_EQ(start_result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(listener.status_changed_count, 0);

  charm::ports::NotifyInputReportRequest notify_request;
  notify_request.report.report_id = 1;
  notify_request.report.size = 1;
  auto notify_before_ready = adapter.NotifyInputReport(notify_request);
  EXPECT_EQ(notify_before_ready.status, charm::contracts::ContractStatus::kUnavailable);

  backend_raw->EmitAdvertisingReady();
  EXPECT_EQ(listener.status_changed_count, 1);
  EXPECT_EQ(listener.last_status.state, charm::contracts::AdapterState::kRunning);
}

TEST_F(BleTransportAdapterTest, PeerLifecycleEventsAreSurfacedToListener) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  charm::ports::BlePeerInfo peer{};
  peer.address = {1, 2, 3, 4, 5, 6};
  adapter.OnPeerConnected(peer);
  adapter.OnPeerDisconnected(peer);

  EXPECT_EQ(listener.connected_count, 1);
  EXPECT_EQ(listener.disconnected_count, 1);
}

TEST_F(BleTransportAdapterTest, NotifyInputReportUsesBackendWhenPeerAndReportChannelAreReady) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  charm::ports::BlePeerInfo peer{};
  adapter.OnPeerConnected(peer);
  adapter.OnReportChannelReady(7, 11, 13, false);

  std::uint8_t bytes[4] = {1, 2, 3, 4};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 2;
  req.report.bytes = bytes;
  req.report.size = 4;

  auto result = adapter.NotifyInputReport(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(backend_raw->configure_calls, 1);
  EXPECT_EQ(backend_raw->send_calls, 1);
  EXPECT_EQ(backend_raw->last_report_size, 4u);
}

TEST_F(BleTransportAdapterTest, StackCallbacksDriveOrderingForReportChannel) {
  backend_raw->use_stack_callbacks = true;
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  backend_raw->EmitAdvertisingReady();

  std::uint8_t bytes[2] = {5, 6};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 8;
  req.report.bytes = bytes;
  req.report.size = 2;

  backend_raw->EmitReportChannelReady(3, 10, 99, false);
  auto without_peer = adapter.NotifyInputReport(req);
  EXPECT_EQ(without_peer.status, charm::contracts::ContractStatus::kUnavailable);
  EXPECT_EQ(backend_raw->send_calls, 0);

  charm::ports::BlePeerInfo peer{};
  peer.address = {9, 8, 7, 6, 5, 4};
  backend_raw->EmitPeerConnected(peer);

  auto after_connect_before_ready = adapter.NotifyInputReport(req);
  EXPECT_EQ(after_connect_before_ready.status, charm::contracts::ContractStatus::kUnavailable);
  EXPECT_EQ(backend_raw->send_calls, 0);

  backend_raw->EmitReportChannelReady(3, 10, 99, false);
  auto with_peer = adapter.NotifyInputReport(req);
  EXPECT_EQ(with_peer.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(backend_raw->send_calls, 1);

  backend_raw->EmitReportChannelClosed();
  auto after_close = adapter.NotifyInputReport(req);
  EXPECT_EQ(after_close.status, charm::contracts::ContractStatus::kUnavailable);
}

TEST_F(BleTransportAdapterTest, NotifyInputReportSurfacesTransportFailure) {
  backend_raw->send_result = false;
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  charm::ports::BlePeerInfo peer{};
  adapter.OnPeerConnected(peer);
  adapter.OnReportChannelReady(7, 11, 13, true);

  std::uint8_t bytes[2] = {9, 9};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 3;
  req.report.bytes = bytes;
  req.report.size = 2;

  auto result = adapter.NotifyInputReport(req);
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kTransportFailure);
  EXPECT_EQ(listener.last_status.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(backend_raw->stop_calls, 1);
  EXPECT_EQ(backend_raw->start_calls, 2);
}

TEST_F(BleTransportAdapterTest, NotifyTransportFailureFailClosesWhenRecoveryFails) {
  backend_raw->send_result = false;
  backend_raw->stop_result = false;

  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);
  charm::ports::BlePeerInfo peer{};
  adapter.OnPeerConnected(peer);
  adapter.OnReportChannelReady(9, 12, 18, true);

  std::uint8_t bytes[1] = {7};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 4;
  req.report.bytes = bytes;
  req.report.size = 1;

  auto first = adapter.NotifyInputReport(req);
  EXPECT_EQ(first.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(backend_raw->stop_calls, 1);

  auto second = adapter.NotifyInputReport(req);
  EXPECT_EQ(second.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(BleTransportAdapterTest, LifecycleErrorAttemptsRecovery) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  backend_raw->EmitLifecycleError(77);

  EXPECT_EQ(backend_raw->stop_calls, 1);
  EXPECT_EQ(backend_raw->start_calls, 2);
  EXPECT_EQ(listener.last_status.status, charm::contracts::ContractStatus::kFailed);
}

TEST_F(BleTransportAdapterTest, StackDisconnectClosesReportChannelBeforeNotify) {
  backend_raw->use_stack_callbacks = true;
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);
  backend_raw->EmitAdvertisingReady();

  charm::ports::BlePeerInfo peer{};
  backend_raw->EmitPeerConnected(peer);
  backend_raw->EmitReportChannelReady(4, 5, 6, false);

  std::uint8_t bytes[1] = {1};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 9;
  req.report.bytes = bytes;
  req.report.size = 1;
  EXPECT_EQ(adapter.NotifyInputReport(req).status, charm::contracts::ContractStatus::kOk);

  backend_raw->EmitPeerDisconnected(peer);
  const auto after_disconnect = adapter.NotifyInputReport(req);
  EXPECT_EQ(after_disconnect.status, charm::contracts::ContractStatus::kUnavailable);
}

TEST_F(BleTransportAdapterTest, RecoveryExhaustionFailClosesAfterBoundedAttempts) {
  backend_raw->use_stack_callbacks = true;
  backend_raw->send_result = false;
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);
  backend_raw->EmitAdvertisingReady();
  charm::ports::BlePeerInfo peer{};
  backend_raw->EmitPeerConnected(peer);
  backend_raw->EmitReportChannelReady(1, 2, 3, true);

  std::uint8_t bytes[1] = {2};
  charm::ports::NotifyInputReportRequest req;
  req.report.report_id = 4;
  req.report.bytes = bytes;
  req.report.size = 1;

  auto first = adapter.NotifyInputReport(req);
  EXPECT_EQ(first.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(backend_raw->stop_calls, 1);
  EXPECT_EQ(backend_raw->start_calls, 2);

  backend_raw->EmitPeerConnected(peer);
  backend_raw->EmitReportChannelReady(1, 2, 3, true);
  auto second = adapter.NotifyInputReport(req);
  EXPECT_EQ(second.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(backend_raw->stop_calls, 2);
  EXPECT_EQ(backend_raw->start_calls, 3);

  backend_raw->EmitPeerConnected(peer);
  backend_raw->EmitReportChannelReady(1, 2, 3, true);
  auto third = adapter.NotifyInputReport(req);
  EXPECT_EQ(third.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(backend_raw->stop_calls, 3);
  EXPECT_EQ(backend_raw->start_calls, 3);

  auto after_exhaustion = adapter.NotifyInputReport(req);
  EXPECT_EQ(after_exhaustion.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(BleTransportAdapterTest, BondingMaterialRoundTripAndClear) {
  std::uint8_t material[3] = {1, 4, 9};
  adapter.SetBondingMaterial(material, 3);

  const auto ref = adapter.GetBondingMaterial();
  ASSERT_NE(ref.bytes, nullptr);
  ASSERT_EQ(ref.size, 3u);
  EXPECT_EQ(ref.bytes[0], 1);
  EXPECT_EQ(ref.bytes[1], 4);
  EXPECT_EQ(ref.bytes[2], 9);

  adapter.ClearBondingMaterial();
  const auto cleared = adapter.GetBondingMaterial();
  EXPECT_EQ(cleared.bytes, nullptr);
  EXPECT_EQ(cleared.size, 0u);
}

TEST_F(BleTransportAdapterTest, StackPeerConnectCachesBondingAddressMaterial) {
  charm::contracts::StartRequest start_req;
  adapter.Start(start_req);

  charm::ports::BlePeerInfo peer{};
  peer.address = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
  adapter.OnStackPeerConnected(peer);

  const auto bonded = adapter.GetBondingMaterial();
  ASSERT_NE(bonded.bytes, nullptr);
  ASSERT_EQ(bonded.size, peer.address.size());
  for (std::size_t i = 0; i < peer.address.size(); ++i) {
    EXPECT_EQ(bonded.bytes[i], peer.address[i]);
  }
}

}  // namespace charm::platform::test
