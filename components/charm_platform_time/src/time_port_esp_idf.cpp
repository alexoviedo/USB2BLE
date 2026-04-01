#include "charm/platform/time_port_esp_idf.hpp"

#include <esp_timer.h>

namespace charm::platform {

charm::ports::GetTimeResult TimePortEspIdf::GetTime(const charm::ports::GetTimeRequest&) const {
  return charm::ports::GetTimeResult{
      .status = charm::contracts::ContractStatus::kOk,
      .fault_code = {},
      .timestamp = charm::contracts::Timestamp{.ticks = static_cast<std::uint64_t>(esp_timer_get_time())},
  };
}

}  // namespace charm::platform
