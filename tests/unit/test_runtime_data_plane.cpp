#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <string_view>
#include <vector>

#include "charm/app/runtime_data_plane.hpp"
#include "charm/core/config_compiler.hpp"
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

  charm::contracts::SelectProfileResult SelectProfile(
      const charm::contracts::SelectProfileRequest& request) override {
    ++select_profile_calls_;
    last_selected_profile_ = request.profile_id;
    return select_profile_result_;
  }

  charm::ports::NotifyInputReportResult NotifyInputReport(
      const charm::ports::NotifyInputReportRequest& request) override {
    ++notify_calls_;
    last_report_id_ = request.report.report_id;
    last_size_ = request.report.size;
    if (request.report.bytes != nullptr && request.report.size > 0) {
      last_bytes_.assign(request.report.bytes,
                         request.report.bytes + request.report.size);
      history_.push_back(last_bytes_);
    }
    return notify_result_;
  }

  void SetListener(charm::ports::BleTransportPortListener* listener) override {
    listener_ = listener;
  }

  void SetNotifyResult(charm::ports::NotifyInputReportResult result) {
    notify_result_ = result;
  }

  void SetSelectProfileResult(charm::contracts::SelectProfileResult result) {
    select_profile_result_ = result;
  }

  std::size_t notify_calls() const { return notify_calls_; }
  std::size_t select_profile_calls() const { return select_profile_calls_; }
  std::size_t last_size() const { return last_size_; }
  charm::contracts::ReportId last_report_id() const { return last_report_id_; }
  charm::contracts::ProfileId last_selected_profile() const {
    return last_selected_profile_;
  }
  const std::vector<std::uint8_t>& last_bytes() const { return last_bytes_; }
  const std::vector<std::vector<std::uint8_t>>& history() const {
    return history_;
  }

 private:
  charm::contracts::SelectProfileResult select_profile_result_{
      charm::contracts::ContractStatus::kOk, {}};
  charm::ports::NotifyInputReportResult notify_result_{
      charm::contracts::ContractStatus::kOk, {}};
  charm::ports::BleTransportPortListener* listener_{nullptr};
  std::size_t select_profile_calls_{0};
  std::size_t notify_calls_{0};
  std::size_t last_size_{0};
  charm::contracts::ReportId last_report_id_{0};
  charm::contracts::ProfileId last_selected_profile_{};
  std::vector<std::uint8_t> last_bytes_{};
  std::vector<std::vector<std::uint8_t>> history_{};
};

struct __attribute__((packed)) GenericGamepadReport {
  std::uint8_t buttons_low{0};
  std::uint8_t buttons_high{0};
  std::uint8_t hat{0};
  std::int8_t left_x{0};
  std::int8_t left_y{0};
  std::int8_t right_x{0};
  std::int8_t right_y{0};
  std::uint8_t left_trigger{0};
  std::uint8_t right_trigger{0};
};

struct __attribute__((packed)) WirelessXboxControllerReport {
  std::uint16_t buttons{0};
  std::uint8_t dpad{0};
  std::uint8_t left_trigger{0};
  std::uint8_t right_trigger{0};
  std::int16_t left_x{0};
  std::int16_t left_y{0};
  std::int16_t right_x{0};
  std::int16_t right_y{0};
};

constexpr std::string_view kCompiledMappingDocument = R"json({
  "version": 1,
  "global": {
    "scale": 1.0,
    "deadzone": 0.0,
    "clamp_min": -1.0,
    "clamp_max": 1.0
  },
  "axes": [
    {
      "target": "look_x",
      "source_index": 0,
      "scale": 1.0,
      "deadzone": 0.0,
      "invert": false
    },
    {
      "target": "look_y",
      "source_index": 1,
      "scale": 1.0,
      "deadzone": 0.0,
      "invert": false
    }
  ],
  "buttons": []
})json";

struct RuntimeHarness {
  RuntimeHarness()
      : state_store({1}),
        mapping_engine(state_store),
        mapping_bundle_loader(&mapping_bundle_validator),
        runtime_data_plane(usb_host, ble_transport, registry, parser,
                           decode_plan_builder, decoder, mapping_engine,
                           mapping_bundle_loader, profile_manager, supervisor) {
    usb_host.SetListener(&runtime_data_plane);
  }

  charm::core::CompiledMappingBundle CompileBundle(std::string_view document) {
    const auto result = config_compiler.CompileConfig(
        {.document =
             {.bytes = reinterpret_cast<const std::uint8_t*>(document.data()),
              .size = document.size()}});
    EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
    return result.bundle;
  }

  charm::test_support::FakeUsbHostPort usb_host;
  CapturingBleTransport ble_transport;
  charm::core::InMemoryDeviceRegistry registry;
  charm::core::DefaultHidDescriptorParser parser;
  charm::core::DefaultDecodePlanBuilder decode_plan_builder;
  charm::core::DefaultHidDecoder decoder;
  charm::core::CanonicalLogicalStateStore state_store;
  charm::core::DefaultMappingEngine mapping_engine;
  charm::core::DefaultMappingBundleValidator mapping_bundle_validator;
  charm::core::DefaultMappingBundleLoader mapping_bundle_loader;
  charm::core::DefaultConfigCompiler config_compiler;
  charm::core::CanonicalProfileManager profile_manager;
  charm::core::DefaultSupervisor supervisor;
  charm::app::RuntimeDataPlane runtime_data_plane;
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

std::vector<std::uint8_t> MakeHotasDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x04,        // Usage (Joystick)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x02,        // Usage Page (Simulation Controls)
      0x09, 0xBA,        // Usage (Rudder)
      0x09, 0xBB,        // Usage (Throttle)
      0x09, 0xC4,        // Usage (Accelerator)
      0x09, 0xC5,        // Usage (Brake)
      0x15, 0x00,        // Logical Minimum (0)
      0x26, 0xFF, 0x03,  // Logical Maximum (1023)
      0x75, 0x10,        // Report Size (16)
      0x95, 0x04,        // Report Count (4)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0xC0,              // End Collection
  };
}

std::vector<std::uint8_t> MakeCompactKeyboardDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x06,        // Usage (Keyboard)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x07,        // Usage Page (Keyboard)
      0x19, 0x04,        // Usage Minimum (4)
      0x29, 0x07,        // Usage Maximum (7)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x07,        // Logical Maximum (7)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x02,        // Report Count (2)
      0x81, 0x00,        // Input (Data,Array,Abs)
      0xC0,              // End Collection
  };
}

std::vector<std::uint8_t> MakeMouseDescriptor() {
  return {
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x02,        // Usage (Mouse)
      0xA1, 0x01,        // Collection (Application)
      0x09, 0x01,        // Usage (Pointer)
      0xA1, 0x00,        // Collection (Physical)
      0x05, 0x09,        // Usage Page (Button)
      0x19, 0x01,        // Usage Minimum (1)
      0x29, 0x03,        // Usage Maximum (3)
      0x15, 0x00,        // Logical Minimum (0)
      0x25, 0x01,        // Logical Maximum (1)
      0x75, 0x01,        // Report Size (1)
      0x95, 0x03,        // Report Count (3)
      0x81, 0x02,        // Input (Data,Var,Abs)
      0x75, 0x05,        // Report Size (5)
      0x95, 0x01,        // Report Count (1)
      0x81, 0x03,        // Input (Const,Var,Abs)
      0x05, 0x01,        // Usage Page (Generic Desktop)
      0x09, 0x30,        // Usage (X)
      0x09, 0x31,        // Usage (Y)
      0x09, 0x38,        // Usage (Wheel)
      0x15, 0x81,        // Logical Minimum (-127)
      0x25, 0x7F,        // Logical Maximum (127)
      0x75, 0x08,        // Report Size (8)
      0x95, 0x03,        // Report Count (3)
      0x81, 0x06,        // Input (Data,Var,Rel)
      0xC0,              // End Collection
      0xC0,              // End Collection
  };
}

TEST(RuntimeDataPlaneTest, ReportPathWiresThroughToBleNotify) {
  RuntimeHarness harness;

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000011, 0x20, 0x40};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{42};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 100;
  report.bytes = report_bytes.data();

  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.notify_calls(), 1u);
  EXPECT_EQ(harness.ble_transport.select_profile_calls(), 1u);
  EXPECT_EQ(harness.ble_transport.last_selected_profile().value, 1u);
  EXPECT_EQ(harness.ble_transport.last_report_id(), 1u);
  EXPECT_GT(harness.ble_transport.last_size(), 0u);
}

TEST(RuntimeDataPlaneTest, MalformedReportIsFailSafeAndDoesNotNotifyBle) {
  RuntimeHarness harness;

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 2> report_bytes{0x01, 0x20};
  charm::contracts::RawHidReportRef malformed_report{};
  malformed_report.device_handle = charm::contracts::DeviceHandle{7};
  malformed_report.interface_handle = charm::contracts::InterfaceHandle{42};
  malformed_report.report_meta.report_id = 0;
  malformed_report.report_meta.declared_length = 3;
  malformed_report.byte_length = report_bytes.size();
  malformed_report.timestamp.ticks = 101;
  malformed_report.bytes = report_bytes.data();

  harness.usb_host.EmitReport(malformed_report);

  EXPECT_EQ(harness.ble_transport.notify_calls(), 0u);
}

TEST(RuntimeDataPlaneTest, NotifyFailureDoesNotCauseUnboundedRetryLoop) {
  RuntimeHarness harness;
  harness.ble_transport.SetNotifyResult(
      {charm::contracts::ContractStatus::kFailed, {}});

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{42}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{7};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000001, 0x11, 0x22};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{42};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 102;
  report.bytes = report_bytes.data();

  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.notify_calls(), 1u);
}

TEST(RuntimeDataPlaneTest, UnknownInterfaceReportIsIgnoredDeterministically) {
  RuntimeHarness harness;

  std::array<std::uint8_t, 3> report_bytes{0b00000011, 0x20, 0x40};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{7};
  report.interface_handle = charm::contracts::InterfaceHandle{999};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 100;
  report.bytes = report_bytes.data();

  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.notify_calls(), 0u);
}

TEST(RuntimeDataPlaneTest, HotasSimulationControlsFlowThroughToBleNotify) {
  RuntimeHarness harness;

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{52}});

  const auto descriptor = MakeHotasDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{9};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 8> report_bytes{0x00, 0x02, 0xFF, 0x03,
                                           0x00, 0x01, 0x80, 0x00};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{9};
  report.interface_handle = charm::contracts::InterfaceHandle{52};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 500;
  report.bytes = report_bytes.data();

  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.notify_calls(), 1u);
  EXPECT_GT(harness.ble_transport.last_size(), 0u);
}

TEST(RuntimeDataPlaneTest, KeyboardArrayPressAndReleaseRemainDeterministic) {
  RuntimeHarness harness;

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{53}});

  const auto descriptor = MakeCompactKeyboardDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{10};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 2> press_report{0x04, 0x00};
  charm::contracts::RawHidReportRef press{};
  press.device_handle = charm::contracts::DeviceHandle{10};
  press.interface_handle = charm::contracts::InterfaceHandle{53};
  press.report_meta.report_id = 0;
  press.report_meta.declared_length = press_report.size();
  press.byte_length = press_report.size();
  press.timestamp.ticks = 600;
  press.bytes = press_report.data();
  harness.usb_host.EmitReport(press);

  std::array<std::uint8_t, 2> release_report{0x00, 0x00};
  auto release = press;
  release.timestamp.ticks = 601;
  release.bytes = release_report.data();
  harness.usb_host.EmitReport(release);

  ASSERT_EQ(harness.ble_transport.notify_calls(), 2u);
  ASSERT_EQ(harness.ble_transport.history().size(), 2u);
  EXPECT_NE(harness.ble_transport.history()[0], harness.ble_transport.history()[1]);
}

TEST(RuntimeDataPlaneTest, MouseRelativeMotionAndZeroResetNotifyCleanly) {
  RuntimeHarness harness;

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{54}});

  const auto descriptor = MakeMouseDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{11};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 4> move_report{0b00000001, 10,
                                          static_cast<std::uint8_t>(-5), 1};
  charm::contracts::RawHidReportRef move{};
  move.device_handle = charm::contracts::DeviceHandle{11};
  move.interface_handle = charm::contracts::InterfaceHandle{54};
  move.report_meta.report_id = 0;
  move.report_meta.declared_length = move_report.size();
  move.byte_length = move_report.size();
  move.timestamp.ticks = 700;
  move.bytes = move_report.data();
  harness.usb_host.EmitReport(move);

  std::array<std::uint8_t, 4> idle_report{0, 0, 0, 0};
  auto idle = move;
  idle.timestamp.ticks = 701;
  idle.bytes = idle_report.data();
  harness.usb_host.EmitReport(idle);

  ASSERT_EQ(harness.ble_transport.notify_calls(), 2u);
  ASSERT_EQ(harness.ble_transport.history().size(), 2u);
  EXPECT_NE(harness.ble_transport.history()[0], harness.ble_transport.history()[1]);
}

TEST(RuntimeDataPlaneTest, ActiveCompiledBundleOverridesDefaultAxisAssignment) {
  RuntimeHarness harness;
  const auto bundle = harness.CompileBundle(kCompiledMappingDocument);
  ASSERT_EQ(harness.mapping_bundle_loader.Load({.bundle = &bundle}).status,
            charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(harness.supervisor.ActivateMappingBundle({.mapping_bundle = bundle.bundle_ref}).status,
            charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(harness.supervisor.SelectProfile({.profile_id = {1}}).status,
            charm::contracts::ContractStatus::kOk);

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{55}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{12};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0, 64, 0};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{12};
  report.interface_handle = charm::contracts::InterfaceHandle{55};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 800;
  report.bytes = report_bytes.data();
  harness.usb_host.EmitReport(report);

  ASSERT_EQ(harness.ble_transport.notify_calls(), 1u);
  ASSERT_EQ(harness.ble_transport.last_bytes().size(), sizeof(GenericGamepadReport));

  GenericGamepadReport encoded{};
  std::memcpy(&encoded, harness.ble_transport.last_bytes().data(), sizeof(encoded));
  EXPECT_EQ(encoded.left_x, 0);
  EXPECT_EQ(encoded.left_y, 0);
  EXPECT_NE(encoded.right_x, 0);
  EXPECT_NE(encoded.right_y, 0);
}

TEST(RuntimeDataPlaneTest, SelectedXboxProfileProducesXboxReportShape) {
  RuntimeHarness harness;
  ASSERT_EQ(harness.supervisor.SelectProfile(
                {.profile_id = charm::core::kWirelessXboxControllerProfileId})
                .status,
            charm::contracts::ContractStatus::kOk);

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{56}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{13};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000001, 0x7F, 0x00};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{13};
  report.interface_handle = charm::contracts::InterfaceHandle{56};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 810;
  report.bytes = report_bytes.data();
  harness.usb_host.EmitReport(report);

  ASSERT_EQ(harness.ble_transport.notify_calls(), 1u);
  EXPECT_EQ(harness.ble_transport.select_profile_calls(), 1u);
  EXPECT_EQ(harness.ble_transport.last_selected_profile().value,
            charm::core::kWirelessXboxControllerProfileId.value);
  EXPECT_EQ(harness.ble_transport.last_report_id(), 2u);
  ASSERT_EQ(harness.ble_transport.last_bytes().size(),
            sizeof(WirelessXboxControllerReport));

  WirelessXboxControllerReport encoded{};
  std::memcpy(&encoded, harness.ble_transport.last_bytes().data(), sizeof(encoded));
  EXPECT_NE(encoded.buttons, 0u);
  EXPECT_TRUE(encoded.left_x != 0 || encoded.left_y != 0 || encoded.right_x != 0 ||
              encoded.right_y != 0);
}

TEST(RuntimeDataPlaneTest, UnsupportedSelectedProfileProducesNoBleNotify) {
  RuntimeHarness harness;
  ASSERT_EQ(harness.supervisor.SelectProfile({.profile_id = {999}}).status,
            charm::contracts::ContractStatus::kOk);

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{57}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{14};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000001, 0x10, 0x20};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{14};
  report.interface_handle = charm::contracts::InterfaceHandle{57};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 811;
  report.bytes = report_bytes.data();
  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.select_profile_calls(), 0u);
  EXPECT_EQ(harness.ble_transport.notify_calls(), 0u);
}

TEST(RuntimeDataPlaneTest, BleProfileSelectionFailureSuppressesNotify) {
  RuntimeHarness harness;
  harness.ble_transport.SetSelectProfileResult(
      {charm::contracts::ContractStatus::kRejected,
       {.category = charm::contracts::ErrorCategory::kUnsupportedCapability,
        .reason = 7}});

  harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                            charm::contracts::InterfaceHandle{58}});

  const auto descriptor = MakeSimpleGamepadDescriptor();
  charm::ports::InterfaceDescriptorRef interface_desc{};
  interface_desc.device_handle = charm::contracts::DeviceHandle{15};
  interface_desc.interface_number = 1;
  interface_desc.descriptor.bytes = descriptor.data();
  interface_desc.descriptor.size = descriptor.size();
  harness.usb_host.EmitInterfaceDescriptor(interface_desc);

  std::array<std::uint8_t, 3> report_bytes{0b00000001, 0x44, 0x55};
  charm::contracts::RawHidReportRef report{};
  report.device_handle = charm::contracts::DeviceHandle{15};
  report.interface_handle = charm::contracts::InterfaceHandle{58};
  report.report_meta.report_id = 0;
  report.report_meta.declared_length = report_bytes.size();
  report.byte_length = report_bytes.size();
  report.timestamp.ticks = 812;
  report.bytes = report_bytes.data();
  harness.usb_host.EmitReport(report);

  EXPECT_EQ(harness.ble_transport.select_profile_calls(), 1u);
  EXPECT_EQ(harness.ble_transport.last_selected_profile().value, 1u);
  EXPECT_EQ(harness.ble_transport.notify_calls(), 0u);
}

namespace {

std::vector<std::uint8_t> RunDualInterfaceScenario(bool first_interface_a) {
  RuntimeHarness harness;

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
    harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                              charm::contracts::InterfaceHandle{42}});
    harness.usb_host.EmitInterfaceDescriptor(iface_a);
    harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                              charm::contracts::InterfaceHandle{43}});
    harness.usb_host.EmitInterfaceDescriptor(iface_b);
  } else {
    harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                              charm::contracts::InterfaceHandle{43}});
    harness.usb_host.EmitInterfaceDescriptor(iface_b);
    harness.usb_host.SetClaimInterfaceResult({charm::contracts::ContractStatus::kOk, {},
                                              charm::contracts::InterfaceHandle{42}});
    harness.usb_host.EmitInterfaceDescriptor(iface_a);
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

  harness.usb_host.EmitReport(raw_a);
  harness.usb_host.EmitReport(raw_b);
  EXPECT_GE(harness.ble_transport.notify_calls(), 2u);
  return harness.ble_transport.last_bytes();
}

}  // namespace

TEST(RuntimeDataPlaneTest, MultiDeviceMergeIsDeterministicAcrossEnumerationOrder) {
  const auto ordered = RunDualInterfaceScenario(true);
  const auto reversed = RunDualInterfaceScenario(false);
  EXPECT_EQ(ordered, reversed);
}

}  // namespace
