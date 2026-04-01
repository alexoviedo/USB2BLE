#pragma once

#include "charm/ports/time_port.hpp"

namespace charm::platform {

class TimePortEspIdf final : public charm::ports::TimePort {
 public:
  ~TimePortEspIdf() override = default;

  charm::ports::GetTimeResult GetTime(const charm::ports::GetTimeRequest& request) const override;
};

}  // namespace charm::platform
