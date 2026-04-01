#pragma once

#include <cstdint>

#include "charm/contracts/common_types.hpp"

namespace charm::contracts {

enum class HidReportType : std::uint8_t {
  kInput = 0,
  kOutput = 1,
  kFeature = 2,
};

struct ReportMeta {
  ReportId report_id{0};
  HidReportType report_type{HidReportType::kInput};
  std::uint16_t declared_length{0};
};

}  // namespace charm::contracts
