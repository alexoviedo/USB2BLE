#pragma once

#include <cstdint>

namespace charm::contracts {

enum class ErrorCategory : std::uint8_t {
  kInvalidRequest = 0,
  kInvalidState = 1,
  kUnsupportedCapability = 2,
  kContractViolation = 3,
  kResourceExhausted = 4,
  kCapacityExceeded = 5,
  kTimeout = 6,
  kIntegrityFailure = 7,
  kPersistenceFailure = 8,
  kAdapterFailure = 9,
  kTransportFailure = 10,
  kDeviceProtocolFailure = 11,
  kConfigurationRejected = 12,
  kRecoveryRequired = 13,
};

struct FaultCode {
  ErrorCategory category{ErrorCategory::kContractViolation};
  std::uint32_t reason{0};
};

}  // namespace charm::contracts
