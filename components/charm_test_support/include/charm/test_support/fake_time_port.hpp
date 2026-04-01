#pragma once

#include "charm/ports/time_port.hpp"

namespace charm::test_support {

class FakeTimePort : public charm::ports::TimePort {
 public:
  void SetNextTime(charm::contracts::Timestamp timestamp) { next_time_ = timestamp; }
  void SetResult(charm::contracts::ContractStatus status, charm::contracts::FaultCode fault_code) {
    status_ = status;
    fault_code_ = fault_code;
  }

  charm::ports::GetTimeResult GetTime(const charm::ports::GetTimeRequest&) const override {
    return charm::ports::GetTimeResult{.status = status_, .fault_code = fault_code_, .timestamp = next_time_};
  }

 private:
  charm::contracts::ContractStatus status_{charm::contracts::ContractStatus::kOk};
  charm::contracts::FaultCode fault_code_{};
  charm::contracts::Timestamp next_time_{};
};

}  // namespace charm::test_support
