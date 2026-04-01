#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "charm/app/runtime_data_plane.hpp"
#include "charm/core/decode_plan.hpp"
#include "charm/core/device_registry.hpp"
#include "charm/core/hid_decoder.hpp"
#include "charm/core/hid_semantic_model.hpp"
#include "charm/core/logical_state.hpp"
#include "charm/core/mapping_engine.hpp"
#include "charm/core/profile_manager.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/test_support/fake_usb_host_port.hpp"

namespace {

class CapturingBleTransport final : public charm::ports::BleTransportPort {
 public:
  charm::contracts::StartResult Start(const charm::contracts::StartRequest&) override {
    return {charm::contracts::ContractStatus::kOk, {}};
  }

  charm::contracts::StopResult Stop(const charm::contracts::StopRequest&) override {
    return {charm::contracts::ContractStatus::kOk, {}};
  }

  charm::ports::NotifyInputReportResult NotifyInputReport(
      const charm::ports::NotifyInputReportRequest& request) override {
    ++notify_calls_;
    last_report_id_ = request.report.report_id;
    last_size_ = request.report.size;
    if (request.report.bytes != nullptr && request.report.size > 0) {
      last_bytes_.assign(request.report.bytes,
                         request.report.bytes + request.report.size);
    }
    return notify_result_;
  }

  void SetListener(charm::ports::BleTransportPortListener* listener) override {
    listener_ = listener;
  }

  void SetNotifyResult(charm::ports::NotifyInputReportResult result) {
    notify_result_ = result;
  }

  std::size_t notify_calls() const { return notify_calls_; }
  std::size_t last_size() const { return last_size_; }
  charm::contracts::ReportId last_report_id() const { return last_report_id_; }
  const std::vector<std::uint8_t>& last_bytes() const { return last_bytes_; }

 private:
  charm::ports::NotifyInputReportResult notify_result_{
      charm::contracts::ContractStatus::kOk, {}};
  charm::ports::BleTransportPortListener* listener_{nullptr};
  std::size_t notify_calls_{0};
  std::size_t last_size_{0};
  charm::contracts::ReportId last_report_id_{0};
  std::vector<std::uint8_t> last_bytes_{};
};

std::vector<std::uint8_t> MakeSimpleGamepadDescriptor() {
  return {
      0x05, 0x01,       // Usage Page (Generic Desktop)
      0x09, 0x05,       // Usage (Game Pad)
      0xA1, 0x01,       // Collection (Application)
      0x05, 0x09,       // Usage Page (Button)
      0x19, 0x01,       // Usage Min (1)
      0x29, 0x02,       // Usage Max (2)
      0x15, 0x00,       // Logical Min (0)
      0x25, 0x01,       // Logical Max (1)
      0x75, 0x01,       // Report Size (1)
      0x95, 0x02,       // Report Count (2)
      0x81, 0x02,       // Input (Data,Var,Abs)
      0x75, 0x06,       // Report Size (6)
      0x95, 0x01,       // Report Count (1)
      0x81, 0x03,       // Input (Const,Var,Abs)
      0x05, 0x01,       // Usage Page (Generic Desktop)
      0x09, 0x30,       // Usage (X)
      0x09, 0x31,       // Usage (Y)
      0x15, 0x00,       // Logical Min (0)
      0x26, 0xFF, 0x00, // Logical Max (255)
      0x75, 0x08,       // Report Size (8)
      0x95, 0x02,       // Report Count (2)
      0x81, 0x02,       // Input (Data,Var,Abs)
      0xC0,             // End Collection
  };
}

}  // namespace

TEST(RuntimeDataPlaneTest, ReportPathWiresThroughToBleNotify) {
  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store({1});
  charm::core::DefaultMappingEngine mapping_engine(state_store);
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;

  charm::app::RuntimeDataPlane runtime_data_plane(
      usb_host, ble_transport, registry, parser, decode_plan_builder, decoder,
      mapping_engine, profile_manager, supervisor);
  usb_host.SetListener(&runtime_data_plane);

  usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                    charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000011, 0x20, 0x40};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{42};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 100;
  report.bytes = report_bytes.data();

  usb_host.EmitReport(report);

  EXPECT_EQ(ble_transport.notify_calls(), 1u);
  EXPECT_GT(ble_transport.last_size(), 0u);
}

TEST(RuntimeDataPlaneTest, MalformedReportIsFailSafeAndDoesNotNotifyBle) {
  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store({1});
  charm::core::DefaultMappingEngine mapping_engine(state_store);
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;

  charm::app::RuntimeDataPlane runtime_data_plane(
      usb_host, ble_transport, registry, parser, decode_plan_builder, decoder,
      mapping_engine, profile_manager, supervisor);
  usb_host.SetListener(&runtime_data_plane);

  usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                    charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 2> report_bytes{0x01, 0x20};
  charm::contracts::RawHidReportRef malformed_report{};
  malformed_report.device_handle = charm::contracts::DeviceHandle{7};
  malformed_report.interface_handle = charm::contracts::InterfaceHandle{42};
  malformed_report.report_meta.report_id = 0;
  malformed_report.report_meta.declared_length = 3;
  malformed_report.byte_length = report_bytes.size();
  malformed_report.timestamp.ticks = 101;
  malformed_report.bytes = report_bytes.data();

  usb_host.EmitReport(malformed_report);

  EXPECT_EQ(ble_transport.notify_calls(), 0u);
}

TEST(RuntimeDataPlaneTest, NotifyFailureDoesNotCauseUnboundedRetryLoop) {
  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  ble_transport.SetNotifyResult({charm::contracts::ContractStatus::kFailed, {}});

  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store({1});
  charm::core::DefaultMappingEngine mapping_engine(state_store);
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;

  charm::app::RuntimeDataPlane runtime_data_plane(
      usb_host, ble_transport, registry, parser, decode_plan_builder, decoder,
      mapping_engine, profile_manager, supervisor);
  usb_host.SetListener(&runtime_data_plane);

  usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                    charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000001, 0x11, 0x22};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{42};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 102;
  report.bytes = report_bytes.data();

  usb_host.EmitReport(report);

  EXPECT_EQ(ble_transport.notify_calls(), 1u);
}

TEST(RuntimeDataPlaneTest, UnknownInterfaceReportIsIgnoredDeterministically) {
  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store({1});
  charm::core::DefaultMappingEngine mapping_engine(state_store);
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;

  charm::app::RuntimeDataPlane runtime_data_plane(
      usb_host, ble_transport, registry, parser, decode_plan_builder, decoder,
      mapping_engine, profile_manager, supervisor);
  usb_host.SetListener(&runtime_data_plane);

  std::array<std::uint8_t, 3> report_bytes{0b00000011, 0x20, 0x40};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{999};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 100;
  report.bytes = report_bytes.data();

  usb_host.EmitReport(report);

  EXPECT_EQ(ble_transport.notify_calls(), 0u);
}

namespace {

std::vector<std::uint8_t> RunDualInterfaceScenario(bool first_interface_a) {
  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store({1});
  charm::core::DefaultMappingEngine mapping_engine(state_store);
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;

  charm::app::RuntimeDataPlane runtime_data_plane(
      usb_host, ble_transport, registry, parser, decode_plan_builder, decoder,
      mapping_engine, profile_manager, supervisor);
  usb_host.SetListener(&runtime_data_plane);

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef iface_a{};
  iface_a.device_handle = charm::contracts::DeviceHandle{7};
  iface_a.interface_number = 1;
  iface_a.descriptor.bytes = descriptor.data();
  iface_a.descriptor.size = descriptor.size();

  charm::ports::InterfaceDescriptorRef iface_b = iface_a;
  iface_b.device_handle = charm::contracts::DeviceHandle{8};
  iface_b.interface_number = 2;

  if (first_interface_a) {
    usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                      charm::contracts::InterfaceHandle{42}});
    usb_host.EmitInterfaceDescriptor(iface_a);
    usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                      charm::contracts::InterfaceHandle{43}});
    usb_host.EmitInterfaceDescriptor(iface_b);
  } else {
    usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                      charm::contracts::InterfaceHandle{43}});
    usb_host.EmitInterfaceDescriptor(iface_b);
    usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                      charm::contracts::InterfaceHandle{42}});
    usb_host.EmitInterfaceDescriptor(iface_a);
  }

  std::array<std::uint8_t, 3> report_a{0b00000001, 0x20, 0x40};
  charm::contracts::RawHidReportRef raw_a{};
  raw_a.device_handle = iface_a.device_handle;
  raw_a.interface_handle = charm::contracts::InterfaceHandle{42};
  raw_a.report_meta.report_id = 0;
  raw_a.report_meta.declared_length = report_a.size();
  raw_a.byte_length = report_a.size();
  raw_a.timestamp.ticks = 200;
  raw_a.bytes = report_a.data();

  std::array<std::uint8_t, 3> report_b{0b00000010, 0x10, 0x60};
  charm::contracts::RawHidReportRef raw_b{};
  raw_b.device_handle = iface_b.device_handle;
  raw_b.interface_handle = charm::contracts::InterfaceHandle{43};
  raw_b.report_meta.report_id = 0;
  raw_b.report_meta.declared_length = report_b.size();
  raw_b.byte_length = report_b.size();
  raw_b.timestamp.ticks = 201;
  raw_b.bytes = report_b.data();

  usb_host.EmitReport(raw_a);
  usb_host.EmitReport(raw_b);
  EXPECT_GE(ble_transport.notify_calls(), 2u);
  return ble_transport.last_bytes();
}

}  // namespace

TEST(RuntimeDataPlaneTest, MultiDeviceMergeIsDeterministicAcrossEnumerationOrder) {
  const auto ordered = RunDualInterfaceScenario(true);
  const auto reversed = RunDualInterfaceScenario(false);
  EXPECT_EQ(ordered, reversed);
}
