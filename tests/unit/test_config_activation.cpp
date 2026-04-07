#include <gtest/gtest.h>

#include "charm/app/config_activation.hpp"
#include "charm/core/mapping_bundle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace {

class ConfigActivationTest : public ::testing::Test {
 protected:
  ConfigActivationTest() : loader_(&validator_) {}

  charm::core::CompiledMappingBundle MakeBundle() {
    charm::core::CompiledMappingBundle bundle{};
    bundle.bundle_ref.bundle_id = 42;
    bundle.bundle_ref.version = charm::core::kSupportedMappingBundleVersion;
    bundle.entry_count = 1;
    bundle.entries[0] = {
        .source = {123},
        .source_type = charm::contracts::InputElementType::kAxis,
        .target = {charm::core::LogicalElementType::kAxis, 0},
        .scale = charm::core::kMappingScaleOne,
        .offset = 0};
    bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
    return bundle;
  }

  charm::test_support::FakeConfigStorePort fake_store_;
  charm::core::DefaultMappingBundleValidator validator_;
  charm::core::DefaultMappingBundleLoader loader_;
  charm::core::DefaultSupervisor supervisor_;
};

TEST_F(ConfigActivationTest, LoadsAndActivatesConfigWhenStoreOk) {
  auto bundle = MakeBundle();
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kOk;
  mock_result.mapping_bundle = bundle.bundle_ref;
  mock_result.compiled_mapping_bundle =
      reinterpret_cast<const std::uint8_t*>(&bundle);
  mock_result.compiled_mapping_bundle_size = sizeof(bundle);
  mock_result.profile_id.value = 1;
  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, loader_, supervisor_);

  const auto state = supervisor_.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 42u);
  EXPECT_EQ(state.active_profile.profile_id.value, 1u);

  const auto active_bundle_result = loader_.GetActiveBundle({});
  ASSERT_EQ(active_bundle_result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_NE(active_bundle_result.bundle, nullptr);
  EXPECT_EQ(active_bundle_result.bundle->bundle_ref.bundle_id, 42u);
}

TEST_F(ConfigActivationTest, IgnoresActivationWhenStoreFails) {
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kRejected;
  mock_result.fault_code = {charm::contracts::ErrorCategory::kPersistenceFailure, 1};
  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, loader_, supervisor_);

  const auto state = supervisor_.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 0u);
  EXPECT_EQ(state.active_profile.profile_id.value, 0u);
  EXPECT_EQ(loader_.GetActiveBundle({}).status,
            charm::contracts::ContractStatus::kUnavailable);
}

TEST_F(ConfigActivationTest, ActivatesLegacyPersistedConfigWhenCompiledBundleBlobIsMissing) {
  auto bundle = MakeBundle();
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kOk;
  mock_result.mapping_bundle = bundle.bundle_ref;
  mock_result.profile_id.value = 1;
  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, loader_, supervisor_);

  const auto state = supervisor_.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 42u);
  EXPECT_EQ(state.active_profile.profile_id.value, 1u);
  EXPECT_EQ(loader_.GetActiveBundle({}).status,
            charm::contracts::ContractStatus::kUnavailable);
}

TEST_F(ConfigActivationTest, IgnoresActivationWhenCompiledBundleBlobIsMalformed) {
  auto bundle = MakeBundle();
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kOk;
  mock_result.mapping_bundle = bundle.bundle_ref;
  mock_result.compiled_mapping_bundle =
      reinterpret_cast<const std::uint8_t*>(&bundle);
  mock_result.compiled_mapping_bundle_size = sizeof(bundle) - 1;
  mock_result.profile_id.value = 1;
  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, loader_, supervisor_);

  const auto state = supervisor_.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 0u);
  EXPECT_EQ(state.active_profile.profile_id.value, 0u);
  EXPECT_EQ(loader_.GetActiveBundle({}).status,
            charm::contracts::ContractStatus::kUnavailable);
}

}  // namespace
