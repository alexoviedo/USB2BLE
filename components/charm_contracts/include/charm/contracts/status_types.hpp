#pragma once

#include <cstdint>

namespace charm::contracts {

enum class ContractStatus : std::uint8_t {
  kUnspecified = 0,
  kOk = 1,
  kRejected = 2,
  kUnavailable = 3,
  kFailed = 4,
};

enum class FaultSeverity : std::uint8_t {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
  kFatal = 3,
};

}  // namespace charm::contracts
