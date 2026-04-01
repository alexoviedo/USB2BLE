#include <gtest/gtest.h>

#include "charm/app/config_activation.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/test_support/fake_config_store_port.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/status_types.hpp"

namespace {

class ConfigActivationTest : public ::testing::Test {
 protected:
  charm::test_support::FakeConfigStorePort fake_store_;
  charm::core::DefaultSupervisor supervisor_;
};

TEST_F(ConfigActivationTest, LoadsAndActivatesConfigWhenStoreOk) {
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kOk;

  // Set up mock mapping bundle
  charm::contracts::MappingBundleRef mock_bundle{};
  mock_bundle.bundle_id = 42;
  mock_result.mapping_bundle = mock_bundle;

  // Set up mock profile
  charm::contracts::ProfileId mock_profile{};
  mock_profile.value = 1337;
  mock_result.profile_id = mock_profile;

  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, supervisor_);

  auto state = supervisor_.GetState();
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 42);
  EXPECT_EQ(state.active_profile.profile_id.value, 1337);
}

TEST_F(ConfigActivationTest, IgnoresActivationWhenStoreFails) {
  charm::contracts::LoadConfigResult mock_result{};
  mock_result.status = charm::contracts::ContractStatus::kRejected;
  mock_result.fault_code = {charm::contracts::ErrorCategory::kPersistenceFailure, 1};

  // Set up mock mapping bundle
  charm::contracts::MappingBundleRef mock_bundle{};
  mock_bundle.bundle_id = 42;
  mock_result.mapping_bundle = mock_bundle;

  // Set up mock profile
  charm::contracts::ProfileId mock_profile{};
  mock_profile.value = 1337;
  mock_result.profile_id = mock_profile;

  fake_store_.SetLoadConfigResult(mock_result);

  charm::app::ActivatePersistedConfig(fake_store_, supervisor_);

  auto state = supervisor_.GetState();
  // Should remain uninitialized/default because load failed
  EXPECT_EQ(state.active_mapping_bundle.mapping_bundle.bundle_id, 0);
  EXPECT_EQ(state.active_profile.profile_id.value, 0);
}

}  // namespace
