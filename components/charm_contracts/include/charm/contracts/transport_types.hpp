#pragma once

#include <cstddef>
#include <cstdint>

#include "charm/contracts/report_types.hpp"

namespace charm::contracts {

struct RawDescriptorRef {
  const std::uint8_t* bytes{nullptr};
  std::size_t size{0};
};

struct EncodedInputReport {
  ReportId report_id{0};
  const std::uint8_t* bytes{nullptr};
  std::size_t size{0};
};

}  // namespace charm::contracts
