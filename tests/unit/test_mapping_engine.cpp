#include <gtest/gtest.h>

#include "charm/core/mapping_engine.hpp"
#include "charm/core/logical_state.hpp"
#include "charm/core/mapping_bundle.hpp"

namespace charm::core {

class MappingEngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    profile_id_ = charm::contracts::ProfileId{1};
    state_store_ = std::make_unique<CanonicalLogicalStateStore>(profile_id_);
    engine_ = std::make_unique<DefaultMappingEngine>(*state_store_);
  }

  charm::contracts::ProfileId profile_id_{};
  std::unique_ptr<CanonicalLogicalStateStore> state_store_;
  std::unique_ptr<DefaultMappingEngine> engine_;
};

TEST_F(MappingEngineTest, DirectEventApplicationAxis) {
  CompiledMappingBundle bundle{};
  bundle.bundle_ref.bundle_id = 42;
  bundle.entry_count = 1;

  MappingEntry entry{};
  entry.source.value = 100;
  entry.source_type = charm::contracts::InputElementType::kAxis;
  entry.target.type = LogicalElementType::kAxis;
  entry.target.index = 0;
  entry.scale = 2;
  entry.offset = 5;

  bundle.entries[0] = entry;

  ApplyInputEventRequest request{};

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  request.active_bundle = &bundle;
  request.active_bundle_ref = bundle.bundle_ref;
  request.input_event.element_key_hash.value = 100;
  request.input_event.element_type = charm::contracts::InputElementType::kAxis;
  request.input_event.value = 10;
  request.input_event.timestamp.ticks = 1000;

  auto apply_result = engine_->ApplyInputEvent(request);
  EXPECT_EQ(apply_result.status, charm::contracts::ContractStatus::kOk);

  GetLogicalStateRequest get_req{};
  get_req.profile_id = profile_id_;
  auto get_result = engine_->GetLogicalState(get_req);
  EXPECT_EQ(get_result.status, charm::contracts::ContractStatus::kOk);

  // (10 * 2) + 5 = 25
  EXPECT_EQ(get_result.snapshot.state->axes[0].value, 25);
  EXPECT_EQ(get_result.snapshot.timestamp.ticks, 1000);
}

TEST_F(MappingEngineTest, DirectEventApplicationButton) {
  CompiledMappingBundle bundle{};
  bundle.bundle_ref.bundle_id = 42;
  bundle.entry_count = 1;

  MappingEntry entry{};
  entry.source.value = 200;
  entry.source_type = charm::contracts::InputElementType::kButton;
  entry.target.type = LogicalElementType::kButton;
  entry.target.index = 1;
  entry.scale = 1;
  entry.offset = 0;

  bundle.entries[0] = entry;

  ApplyInputEventRequest request{};

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  request.active_bundle = &bundle;
  request.active_bundle_ref = bundle.bundle_ref;
  request.input_event.element_key_hash.value = 200;
  request.input_event.element_type = charm::contracts::InputElementType::kButton;
  request.input_event.value = 1;
  request.input_event.timestamp.ticks = 2000;

  auto apply_result = engine_->ApplyInputEvent(request);
  EXPECT_EQ(apply_result.status, charm::contracts::ContractStatus::kOk);

  GetLogicalStateRequest get_req{};
  get_req.profile_id = profile_id_;
  auto get_result = engine_->GetLogicalState(get_req);
  EXPECT_EQ(get_result.status, charm::contracts::ContractStatus::kOk);

  EXPECT_TRUE(get_result.snapshot.state->buttons[1].pressed);
  EXPECT_EQ(get_result.snapshot.timestamp.ticks, 2000);
}

TEST_F(MappingEngineTest, MissingBundleRejection) {
  ApplyInputEventRequest request{};
  request.active_bundle = nullptr;
  request.input_event.element_key_hash.value = 100;
  request.input_event.element_type = charm::contracts::InputElementType::kAxis;
  request.input_event.value = 10;
  request.input_event.timestamp.ticks = 1000;

  auto apply_result = engine_->ApplyInputEvent(request);
  EXPECT_EQ(apply_result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(apply_result.fault_code.category, charm::contracts::ErrorCategory::kContractViolation);
}

TEST_F(MappingEngineTest, UnmappedEventIgnored) {
  CompiledMappingBundle bundle{};
  bundle.bundle_ref.bundle_id = 42;
  bundle.entry_count = 1;

  MappingEntry entry{};
  entry.source.value = 100;
  entry.source_type = charm::contracts::InputElementType::kAxis;
  entry.target.type = LogicalElementType::kAxis;
  entry.target.index = 0;
  entry.scale = 1;
  entry.offset = 0;

  bundle.entries[0] = entry;

  ApplyInputEventRequest request{};

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  request.active_bundle = &bundle;
  request.active_bundle_ref = bundle.bundle_ref;
  request.input_event.element_key_hash.value = 999; // Different key
  request.input_event.element_type = charm::contracts::InputElementType::kAxis;
  request.input_event.value = 10;
  request.input_event.timestamp.ticks = 1000;

  auto apply_result = engine_->ApplyInputEvent(request);
  EXPECT_EQ(apply_result.status, charm::contracts::ContractStatus::kOk);

  GetLogicalStateRequest get_req{};
  get_req.profile_id = profile_id_;
  auto get_result = engine_->GetLogicalState(get_req);
  EXPECT_EQ(get_result.status, charm::contracts::ContractStatus::kOk);

  EXPECT_EQ(get_result.snapshot.state->axes[0].value, 0); // Not mapped
}

TEST_F(MappingEngineTest, StateReset) {
  CompiledMappingBundle bundle{};
  bundle.bundle_ref.bundle_id = 42;
  bundle.entry_count = 1;

  MappingEntry entry{};
  entry.source.value = 100;
  entry.source_type = charm::contracts::InputElementType::kAxis;
  entry.target.type = LogicalElementType::kAxis;
  entry.target.index = 0;
  entry.scale = 1;
  entry.offset = 0;

  bundle.entries[0] = entry;

  ApplyInputEventRequest request{};

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  request.active_bundle = &bundle;
  request.active_bundle_ref = bundle.bundle_ref;
  request.input_event.element_key_hash.value = 100;
  request.input_event.element_type = charm::contracts::InputElementType::kAxis;
  request.input_event.value = 50;
  request.input_event.timestamp.ticks = 1000;

  engine_->ApplyInputEvent(request);

  GetLogicalStateRequest get_req{};
  get_req.profile_id = profile_id_;
  auto get_result = engine_->GetLogicalState(get_req);
  EXPECT_EQ(get_result.snapshot.state->axes[0].value, 50);

  ResetLogicalStateRequest reset_req{};
  auto reset_result = engine_->ResetLogicalState(reset_req);
  EXPECT_EQ(reset_result.status, charm::contracts::ContractStatus::kOk);

  auto get_result_after = engine_->GetLogicalState(get_req);
  EXPECT_EQ(get_result_after.snapshot.state->axes[0].value, 0);
}

}  // namespace charm::core
