#pragma once

#include "charm/contracts/events.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/core/logical_state.hpp"
#include "charm/core/mapping_bundle.hpp"

namespace charm::core {

struct ApplyInputEventRequest {
  charm::contracts::InputElementEvent input_event{};
  charm::contracts::MappingBundleRef active_bundle_ref{};
  const CompiledMappingBundle* active_bundle{nullptr};
};

struct ApplyInputEventResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

class MappingEngine {
 public:
  virtual ~MappingEngine() = default;

  virtual ApplyInputEventResult ApplyInputEvent(const ApplyInputEventRequest& request) = 0;
  virtual GetLogicalStateResult GetLogicalState(const GetLogicalStateRequest& request) const = 0;
  virtual ResetLogicalStateResult ResetLogicalState(const ResetLogicalStateRequest& request) = 0;
};

class DefaultMappingEngine : public MappingEngine {
 public:
  explicit DefaultMappingEngine(CanonicalLogicalStateStore& state_store);
  ~DefaultMappingEngine() override = default;

  ApplyInputEventResult ApplyInputEvent(const ApplyInputEventRequest& request) override;
  GetLogicalStateResult GetLogicalState(const GetLogicalStateRequest& request) const override;
  ResetLogicalStateResult ResetLogicalState(const ResetLogicalStateRequest& request) override;

 private:
  CanonicalLogicalStateStore& state_store_;
};

}  // namespace charm::core
