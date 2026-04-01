#include <gtest/gtest.h>
#include "charm/platform/usb_host_adapter.hpp"
#include "charm/ports/usb_host_port.hpp"

class MockUsbHostListener : public charm::ports::UsbHostPortListener {
 public:
  void OnDeviceConnected(const charm::ports::UsbEnumerationInfo& enumeration_info,
                         const charm::ports::DeviceDescriptorRef& device_descriptor) override {
    connected_calls++;
  }
  void OnDeviceDisconnected(charm::contracts::DeviceHandle device_handle) override {
    disconnected_calls++;
  }
  void OnInterfaceDescriptorAvailable(const charm::ports::InterfaceDescriptorRef& interface_descriptor) override {
    interface_calls++;
  }
  void OnReportReceived(const charm::contracts::RawHidReportRef& report_ref) override {
    report_calls++;
  }
  void OnStatusChanged(const charm::ports::UsbHostStatus& status) override {
    last_status = status.status;
    last_state = status.state;
  }

  int connected_calls = 0;
  int disconnected_calls = 0;
  int interface_calls = 0;
  int report_calls = 0;
  charm::contracts::ContractStatus last_status = charm::contracts::ContractStatus::kUnspecified;
  charm::contracts::AdapterState last_state = charm::contracts::AdapterState::kUnknown;
};

class UsbHostAdapterTest : public ::testing::Test {
 protected:
  charm::platform::UsbHostAdapter adapter;
  MockUsbHostListener listener;

  void SetUp() override {
    adapter.SetListener(&listener);
  }
};

TEST_F(UsbHostAdapterTest, StartsSuccessfully) {
  auto result = adapter.Start({});
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(listener.last_state, charm::contracts::AdapterState::kReady);
}

TEST_F(UsbHostAdapterTest, FailsToStartIfAlreadyStarted) {
  adapter.Start({});
  auto result = adapter.Start({});
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(UsbHostAdapterTest, StopsSuccessfully) {
  adapter.Start({});
  auto result = adapter.Stop({});
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(listener.last_state, charm::contracts::AdapterState::kStopped);
}

TEST_F(UsbHostAdapterTest, FailsToStopIfNotStarted) {
  auto result = adapter.Stop({});
  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
}

TEST_F(UsbHostAdapterTest, DispatchesEventsWhenStarted) {
  adapter.Start({});

  charm::ports::UsbEnumerationInfo info;
  charm::ports::DeviceDescriptorRef dev_ref;
  adapter.SimulateDeviceConnected(info, dev_ref);
  EXPECT_EQ(listener.connected_calls, 1);

  adapter.SimulateDeviceDisconnected(charm::contracts::DeviceHandle{});
  EXPECT_EQ(listener.disconnected_calls, 1);

  charm::ports::InterfaceDescriptorRef iface_ref;
  adapter.SimulateInterfaceDescriptorAvailable(iface_ref);
  EXPECT_EQ(listener.interface_calls, 1);

  charm::contracts::RawHidReportRef report_ref;
  adapter.SimulateReportReceived(report_ref);
  EXPECT_EQ(listener.report_calls, 1);
}

TEST_F(UsbHostAdapterTest, DropsEventsWhenNotStarted) {
  charm::ports::UsbEnumerationInfo info;
  charm::ports::DeviceDescriptorRef dev_ref;
  adapter.SimulateDeviceConnected(info, dev_ref);
  EXPECT_EQ(listener.connected_calls, 0);

  adapter.SimulateDeviceDisconnected(charm::contracts::DeviceHandle{});
  EXPECT_EQ(listener.disconnected_calls, 0);

  charm::ports::InterfaceDescriptorRef iface_ref;
  adapter.SimulateInterfaceDescriptorAvailable(iface_ref);
  EXPECT_EQ(listener.interface_calls, 0);

  charm::contracts::RawHidReportRef report_ref;
  adapter.SimulateReportReceived(report_ref);
  EXPECT_EQ(listener.report_calls, 0);
}

TEST_F(UsbHostAdapterTest, ClaimInterfaceReturnsUniqueNonZeroHandle) {
  adapter.Start({});

  charm::ports::ClaimInterfaceRequest req1;
  req1.interface_number = 0;
  auto result1 = adapter.ClaimInterface(req1);
  EXPECT_EQ(result1.status, charm::contracts::ContractStatus::kOk);
  EXPECT_NE(result1.interface_handle.value, 0);

  charm::ports::ClaimInterfaceRequest req2;
  req2.interface_number = 0;
  auto result2 = adapter.ClaimInterface(req2);
  EXPECT_EQ(result2.status, charm::contracts::ContractStatus::kOk);
  EXPECT_NE(result2.interface_handle.value, 0);
  EXPECT_NE(result1.interface_handle.value, result2.interface_handle.value);
}
