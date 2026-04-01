#include <gtest/gtest.h>

#include "charm/app/startup_storage_lifecycle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace {

int g_init_call_count = 0;
int g_erase_call_count = 0;
int g_init_return_values[2] = {0, 0};
int g_erase_return_value = 0;
int g_activate_calls = 0;

int TestInit() {
  const int index = g_init_call_count < 2 ? g_init_call_count : 1;
  ++g_init_call_count;
  return g_init_return_values[index];
}

int TestErase() {
  ++g_erase_call_count;
  return g_erase_return_value;
}

void TestActivate(charm::ports::ConfigStorePort& /*store*/,
                  charm::core::Supervisor& /*supervisor*/) {
  ++g_activate_calls;
}

class StartupStorageLifecycleTest : public ::testing::Test {
 protected:
  static constexpr int kRecoverableNoFreePages = 0x110d;

  void SetUp() override {
    g_init_call_count = 0;
    g_erase_call_count = 0;
    g_init_return_values[0] = 0;
    g_init_return_values[1] = 0;
    g_erase_return_value = 0;
    g_activate_calls = 0;
  }
};

TEST_F(StartupStorageLifecycleTest, InitializeStorageSucceedsOnFirstInit) {
  const charm::app::StorageInitFns fns{&TestInit, &TestErase};
  const auto outcome = charm::app::InitializeStorage(fns);

  EXPECT_TRUE(outcome.ok);
  EXPECT_FALSE(outcome.recovered);
  EXPECT_EQ(g_init_call_count, 1);
  EXPECT_EQ(g_erase_call_count, 0);
}

TEST_F(StartupStorageLifecycleTest, InitializeStorageRecoversViaEraseAndReinit) {
  g_init_return_values[0] = kRecoverableNoFreePages;
  g_init_return_values[1] = 0;
  const charm::app::StorageInitFns fns{&TestInit, &TestErase};
  const auto outcome = charm::app::InitializeStorage(fns);

  EXPECT_TRUE(outcome.ok);
  EXPECT_TRUE(outcome.recovered);
  EXPECT_EQ(g_init_call_count, 2);
  EXPECT_EQ(g_erase_call_count, 1);
}

TEST_F(StartupStorageLifecycleTest, InitializeStorageFailsWhenReinitStillFails) {
  g_init_return_values[0] = kRecoverableNoFreePages;
  g_init_return_values[1] = kRecoverableNoFreePages;
  const charm::app::StorageInitFns fns{&TestInit, &TestErase};
  const auto outcome = charm::app::InitializeStorage(fns);

  EXPECT_FALSE(outcome.ok);
  EXPECT_EQ(outcome.reason, 3u);
  EXPECT_EQ(g_init_call_count, 2);
  EXPECT_EQ(g_erase_call_count, 1);
}

TEST_F(StartupStorageLifecycleTest, InitFailureGatesConfigActivationAndRecordsFault) {
  g_init_return_values[0] = 9;
  charm::test_support::FakeConfigStorePort store;
  charm::core::DefaultSupervisor supervisor;
  const charm::app::StorageInitFns fns{&TestInit, &TestErase};

  const bool started = charm::app::InitializeStorageAndActivate(
      store, supervisor, &TestActivate, fns);
  EXPECT_FALSE(started);
  EXPECT_EQ(g_activate_calls, 0);
  const auto state = supervisor.GetState();
  EXPECT_EQ(state.last_fault.fault_code.category,
            charm::contracts::ErrorCategory::kPersistenceFailure);
  EXPECT_EQ(state.last_fault.fault_code.reason, 1u);
}

TEST_F(StartupStorageLifecycleTest, SuccessPathAllowsConfigActivation) {
  charm::test_support::FakeConfigStorePort store;
  charm::core::DefaultSupervisor supervisor;
  const charm::app::StorageInitFns fns{&TestInit, &TestErase};

  const bool started = charm::app::InitializeStorageAndActivate(
      store, supervisor, &TestActivate, fns);
  EXPECT_TRUE(started);
  EXPECT_EQ(g_activate_calls, 1);
}

}  // namespace
