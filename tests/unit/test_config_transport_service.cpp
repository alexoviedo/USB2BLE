#include <gtest/gtest.h>

#include "charm/app/config_transport_service.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace charm::app::test {

class ConfigTransportServiceTest : public ::testing::Test {
 protected:
  charm::test_support::FakeConfigStorePort store;
  ConfigTransportService service{store};

  charm::contracts::ConfigTransportRequest BaseRequest(
      charm::contracts::ConfigTransportCommand command) {
    charm::contracts::ConfigTransportRequest req{};
    req.protocol_version = ConfigTransportService::kProtocolVersion;
    req.request_id = 42;
    req.command = command;
    req.integrity = ConfigTransportService::kExpectedIntegrity;
    return req;
  }
};

TEST_F(ConfigTransportServiceTest, RejectsUnsupportedProtocolVersion) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kLoad);
  request.protocol_version = 7;
  const auto response = service.HandleRequest(request);
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category, charm::contracts::ErrorCategory::kUnsupportedCapability);
}

TEST_F(ConfigTransportServiceTest, RejectsIntegrityMismatch) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kLoad);
  request.integrity = 0;
  const auto response = service.HandleRequest(request);
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category, charm::contracts::ErrorCategory::kIntegrityFailure);
}

TEST_F(ConfigTransportServiceTest, PersistForwardsToConfigStore) {
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kOk;
  store.SetPersistConfigResult(persist_result);

  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  request.mapping_bundle.bundle_id = 77;
  request.profile_id.value = 3;
  std::uint8_t bonding[2] = {1, 2};
  request.bonding_material = bonding;
  request.bonding_material_size = 2;

  const auto response = service.HandleRequest(request);
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(store.PersistCallCount(), 1);
  EXPECT_EQ(store.LastPersistRequest().mapping_bundle.bundle_id, 77u);
  EXPECT_EQ(store.LastPersistRequest().profile_id.value, 3u);
  EXPECT_EQ(store.LastPersistRequest().bonding_material_size, 2u);
}

TEST_F(ConfigTransportServiceTest, PersistRejectsMissingPayloadShape) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  request.profile_id.value = 4;
  const auto response = service.HandleRequest(request);
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category, charm::contracts::ErrorCategory::kInvalidRequest);
}

TEST_F(ConfigTransportServiceTest, LoadReturnsConfigPayload) {
  charm::contracts::LoadConfigResult load_result{};
  load_result.status = charm::contracts::ContractStatus::kOk;
  load_result.mapping_bundle.bundle_id = 11;
  load_result.profile_id.value = 9;
  store.SetLoadConfigResult(load_result);

  const auto response =
      service.HandleRequest(BaseRequest(charm::contracts::ConfigTransportCommand::kLoad));
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(response.mapping_bundle.bundle_id, 11u);
  EXPECT_EQ(response.profile_id.value, 9u);
}

TEST_F(ConfigTransportServiceTest, ClearMapsResultFromConfigStore) {
  charm::ports::ClearConfigResult clear_result{};
  clear_result.status = charm::contracts::ContractStatus::kFailed;
  clear_result.fault_code.category = charm::contracts::ErrorCategory::kPersistenceFailure;
  clear_result.fault_code.reason = 5;
  store.SetClearConfigResult(clear_result);

  const auto response =
      service.HandleRequest(BaseRequest(charm::contracts::ConfigTransportCommand::kClear));
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kFailed);
  EXPECT_EQ(response.fault_code.category, charm::contracts::ErrorCategory::kPersistenceFailure);
  EXPECT_EQ(store.ClearCallCount(), 1);
}

TEST_F(ConfigTransportServiceTest, GetCapabilitiesDeclaresSerialOnlyFirstPath) {
  const auto response = service.HandleRequest(
      BaseRequest(charm::contracts::ConfigTransportCommand::kGetCapabilities));
  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(response.capabilities.protocol_version, ConfigTransportService::kProtocolVersion);
  EXPECT_TRUE(response.capabilities.supports_persist);
  EXPECT_TRUE(response.capabilities.supports_load);
  EXPECT_TRUE(response.capabilities.supports_clear);
  EXPECT_TRUE(response.capabilities.supports_get_capabilities);
  EXPECT_FALSE(response.capabilities.supports_ble_transport);
}

}  // namespace charm::app::test
