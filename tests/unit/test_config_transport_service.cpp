#include <gtest/gtest.h>

#include <string_view>

#include "charm/app/config_transport_service.hpp"
#include "charm/core/config_compiler.hpp"
#include "charm/core/mapping_bundle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace charm::app::test {

namespace {

constexpr std::string_view kMappingDocument = R"json({
  "version": 1,
  "global": {
    "scale": 1.0,
    "deadzone": 0.08,
    "clamp_min": -1.0,
    "clamp_max": 1.0
  },
  "axes": [
    {
      "target": "move_x",
      "source_index": 0,
      "scale": 1.0,
      "deadzone": 0.08,
      "invert": false
    }
  ],
  "buttons": [
    {
      "target": "action_a",
      "source_index": 0
    }
  ]
})json";

}  // namespace

class ConfigTransportServiceTest : public ::testing::Test {
 protected:
  ConfigTransportServiceTest()
      : loader(&validator),
        service(store, compiler, loader, supervisor) {}

  charm::contracts::ConfigTransportRequest BaseRequest(
      charm::contracts::ConfigTransportCommand command) {
    charm::contracts::ConfigTransportRequest req{};
    req.protocol_version = ConfigTransportService::kProtocolVersion;
    req.request_id = 42;
    req.command = command;
    req.integrity = ConfigTransportService::kExpectedIntegrity;
    return req;
  }

  void AttachMappingDocument(charm::contracts::ConfigTransportRequest* request) {
    request->mapping_document =
        reinterpret_cast<const std::uint8_t*>(kMappingDocument.data());
    request->mapping_document_size = kMappingDocument.size();
  }

  charm::test_support::FakeConfigStorePort store;
  charm::core::DefaultConfigCompiler compiler;
  charm::core::DefaultMappingBundleValidator validator;
  charm::core::DefaultMappingBundleLoader loader;
  charm::core::DefaultSupervisor supervisor;
  ConfigTransportService service;
};

TEST_F(ConfigTransportServiceTest, RejectsUnsupportedProtocolVersion) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kLoad);
  request.protocol_version = 7;

  const auto response = service.HandleRequest(request);

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category,
            charm::contracts::ErrorCategory::kUnsupportedCapability);
}

TEST_F(ConfigTransportServiceTest, RejectsIntegrityMismatch) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kLoad);
  request.integrity = 0;

  const auto response = service.HandleRequest(request);

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category,
            charm::contracts::ErrorCategory::kIntegrityFailure);
}

TEST_F(ConfigTransportServiceTest, PersistCompilesStoresAndActivatesRuntimeBundle) {
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kOk;
  store.SetPersistConfigResult(persist_result);

  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  AttachMappingDocument(&request);
  request.profile_id.value = 1;
  std::uint8_t bonding[2] = {1, 2};
  request.bonding_material = bonding;
  request.bonding_material_size = 2;

  const auto response = service.HandleRequest(request);

  ASSERT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(store.PersistCallCount(), 1);
  EXPECT_EQ(store.LastPersistRequest().profile_id.value, 1u);
  EXPECT_EQ(store.LastPersistRequest().bonding_material_size, 2u);
  EXPECT_EQ(store.LastPersistRequest().compiled_mapping_bundle_size,
            sizeof(charm::core::CompiledMappingBundle));
  EXPECT_NE(store.LastPersistRequest().mapping_bundle.bundle_id, 0u);
  EXPECT_EQ(response.mapping_bundle.bundle_id,
            store.LastPersistRequest().mapping_bundle.bundle_id);
  EXPECT_EQ(response.profile_id.value, 1u);

  const auto active_bundle_result = loader.GetActiveBundle({});
  ASSERT_EQ(active_bundle_result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_NE(active_bundle_result.bundle, nullptr);
  EXPECT_EQ(active_bundle_result.bundle->bundle_ref.bundle_id,
            response.mapping_bundle.bundle_id);

  const auto supervisor_state = supervisor.GetState();
  EXPECT_EQ(supervisor_state.active_mapping_bundle.mapping_bundle.bundle_id,
            response.mapping_bundle.bundle_id);
  EXPECT_EQ(supervisor_state.active_profile.profile_id.value, 1u);
}

TEST_F(ConfigTransportServiceTest,
       PersistCarriesWirelessXboxProfileThroughStorageAndActivation) {
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kOk;
  store.SetPersistConfigResult(persist_result);

  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  AttachMappingDocument(&request);
  request.profile_id.value = 2;

  const auto response = service.HandleRequest(request);

  ASSERT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(store.PersistCallCount(), 1);
  EXPECT_EQ(store.LastPersistRequest().profile_id.value, 2u);
  EXPECT_EQ(response.profile_id.value, 2u);

  const auto active_bundle_result = loader.GetActiveBundle({});
  ASSERT_EQ(active_bundle_result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_NE(active_bundle_result.bundle, nullptr);

  const auto supervisor_state = supervisor.GetState();
  EXPECT_EQ(supervisor_state.active_mapping_bundle.mapping_bundle.bundle_id,
            response.mapping_bundle.bundle_id);
  EXPECT_EQ(supervisor_state.active_profile.profile_id.value, 2u);
}

TEST_F(ConfigTransportServiceTest, PersistRejectsMissingPayloadShape) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  request.profile_id.value = 1;

  const auto response = service.HandleRequest(request);

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category,
            charm::contracts::ErrorCategory::kInvalidRequest);
}

TEST_F(ConfigTransportServiceTest, PersistSurfacesCompilerFailures) {
  auto request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  request.profile_id.value = 1;
  constexpr std::string_view kInvalidDocument = R"json({
    "version": 1,
    "global": {
      "scale": 1.0,
      "deadzone": 0.1,
      "clamp_min": 1.0,
      "clamp_max": -1.0
    },
    "axes": [],
    "buttons": []
  })json";
  request.mapping_document =
      reinterpret_cast<const std::uint8_t*>(kInvalidDocument.data());
  request.mapping_document_size = kInvalidDocument.size();

  const auto response = service.HandleRequest(request);

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(response.fault_code.category,
            charm::contracts::ErrorCategory::kInvalidRequest);
  EXPECT_EQ(store.PersistCallCount(), 0);
}

TEST_F(ConfigTransportServiceTest, LoadReturnsConfigPayload) {
  charm::contracts::LoadConfigResult load_result{};
  load_result.status = charm::contracts::ContractStatus::kOk;
  load_result.mapping_bundle.bundle_id = 11;
  load_result.profile_id.value = 1;
  store.SetLoadConfigResult(load_result);

  const auto response =
      service.HandleRequest(BaseRequest(charm::contracts::ConfigTransportCommand::kLoad));

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(response.mapping_bundle.bundle_id, 11u);
  EXPECT_EQ(response.profile_id.value, 1u);
}

TEST_F(ConfigTransportServiceTest, ClearMapsResultFromConfigStoreAndClearsActiveBundle) {
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kOk;
  store.SetPersistConfigResult(persist_result);

  auto persist_request = BaseRequest(charm::contracts::ConfigTransportCommand::kPersist);
  AttachMappingDocument(&persist_request);
  persist_request.profile_id.value = 1;
  ASSERT_EQ(service.HandleRequest(persist_request).status,
            charm::contracts::ContractStatus::kOk);

  charm::ports::ClearConfigResult clear_result{};
  clear_result.status = charm::contracts::ContractStatus::kOk;
  store.SetClearConfigResult(clear_result);

  const auto response =
      service.HandleRequest(BaseRequest(charm::contracts::ConfigTransportCommand::kClear));

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(store.ClearCallCount(), 1);
  EXPECT_EQ(loader.GetActiveBundle({}).status,
            charm::contracts::ContractStatus::kUnavailable);
  const auto state = supervisor.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 0u);
  EXPECT_EQ(state.active_profile.profile_id.value, 0u);
}

TEST_F(ConfigTransportServiceTest, GetCapabilitiesDeclaresSerialOnlyCompilerPath) {
  const auto response = service.HandleRequest(
      BaseRequest(charm::contracts::ConfigTransportCommand::kGetCapabilities));

  EXPECT_EQ(response.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(response.capabilities.protocol_version,
            ConfigTransportService::kProtocolVersion);
  EXPECT_TRUE(response.capabilities.supports_persist);
  EXPECT_TRUE(response.capabilities.supports_load);
  EXPECT_TRUE(response.capabilities.supports_clear);
  EXPECT_TRUE(response.capabilities.supports_get_capabilities);
  EXPECT_FALSE(response.capabilities.supports_ble_transport);
}

}  // namespace charm::app::test
