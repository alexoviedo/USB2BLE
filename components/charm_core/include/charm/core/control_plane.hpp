#pragma once
#include "charm/contracts/events.hpp"
namespace charm::core {
class ControlPlaneSink {
 public:
  virtual ~ControlPlaneSink() = default;
  virtual void Publish(const charm::contracts::ControlPlaneEvent& event) = 0;
  virtual void Publish(const charm::contracts::FaultEvent& event) = 0;
  virtual void Publish(const charm::contracts::AdapterStatusEvent& event) = 0;
};
class ControlPlaneSource {
 public:
  virtual ~ControlPlaneSource() = default;
  virtual void SetSink(ControlPlaneSink* sink) = 0;
};
}  // namespace charm::core
