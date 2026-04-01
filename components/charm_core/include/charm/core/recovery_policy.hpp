#pragma once

#include "charm/core/control_plane.hpp"
#include "charm/core/supervisor.hpp"

namespace charm::core {

class DefaultRecoveryPolicy : public ControlPlaneSink {
 public:
  explicit DefaultRecoveryPolicy(Supervisor& supervisor);
  ~DefaultRecoveryPolicy() override = default;

  void Publish(const charm::contracts::ControlPlaneEvent& event) override;
  void Publish(const charm::contracts::FaultEvent& event) override;
  void Publish(const charm::contracts::AdapterStatusEvent& event) override;

 private:
  Supervisor& supervisor_;
};

}  // namespace charm::core
