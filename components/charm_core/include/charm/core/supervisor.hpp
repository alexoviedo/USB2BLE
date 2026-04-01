#pragma once
#include <cstdint>
#include "charm/contracts/registry_types.hpp"
#include "charm/contracts/requests.hpp"
namespace charm::contracts {
enum class ModeState : std::uint8_t { kUnknown=0, kConfiguration=1, kRun=2 };
enum class RecoveryState : std::uint8_t { kNone=0, kRequested=1, kRecovering=2 };
}  // namespace charm::contracts
namespace charm::core {
struct SupervisorState {
  charm::contracts::ModeState mode{charm::contracts::ModeState::kUnknown};
  charm::contracts::ActiveProfileRef active_profile{};
  charm::contracts::ActiveMappingBundleRef active_mapping_bundle{};
  charm::contracts::RecoveryState recovery_state{charm::contracts::RecoveryState::kNone};
  charm::contracts::FaultRecordRef last_fault{};
};
class Supervisor {
 public:
  virtual ~Supervisor() = default;
  virtual charm::contracts::StartResult Start(const charm::contracts::StartRequest& request) = 0;
  virtual charm::contracts::StopResult Stop(const charm::contracts::StopRequest& request) = 0;
  virtual charm::contracts::ModeTransitionResult TransitionMode(const charm::contracts::ModeTransitionRequest& request) = 0;
  virtual charm::contracts::ActivateMappingBundleResult ActivateMappingBundle(const charm::contracts::ActivateMappingBundleRequest& request) = 0;
  virtual charm::contracts::SelectProfileResult SelectProfile(const charm::contracts::SelectProfileRequest& request) = 0;
  virtual charm::contracts::RecoveryResult RequestRecovery(const charm::contracts::RecoveryRequest& request) = 0;
  virtual void SetLastFault(const charm::contracts::FaultRecordRef& fault) = 0;
  virtual SupervisorState GetState() const = 0;
};

class DefaultSupervisor : public Supervisor {
 public:
  DefaultSupervisor() = default;
  ~DefaultSupervisor() override = default;

  charm::contracts::StartResult Start(const charm::contracts::StartRequest& request) override;
  charm::contracts::StopResult Stop(const charm::contracts::StopRequest& request) override;
  charm::contracts::ModeTransitionResult TransitionMode(const charm::contracts::ModeTransitionRequest& request) override;
  charm::contracts::ActivateMappingBundleResult ActivateMappingBundle(const charm::contracts::ActivateMappingBundleRequest& request) override;
  charm::contracts::SelectProfileResult SelectProfile(const charm::contracts::SelectProfileRequest& request) override;
  charm::contracts::RecoveryResult RequestRecovery(const charm::contracts::RecoveryRequest& request) override;
  void SetLastFault(const charm::contracts::FaultRecordRef& fault) override;
  SupervisorState GetState() const override;

 private:
  SupervisorState state_{};
};
}  // namespace charm::core
