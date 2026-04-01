#pragma once

#include "charm/contracts/requests.hpp"

namespace charm::ports {

struct GetTimeRequest {};

struct GetTimeResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::Timestamp timestamp{};
};

class TimePort {
 public:
  virtual ~TimePort() = default;

  virtual GetTimeResult GetTime(const GetTimeRequest& request) const = 0;
};

}  // namespace charm::ports
