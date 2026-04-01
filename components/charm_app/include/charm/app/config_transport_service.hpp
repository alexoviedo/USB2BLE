#pragma once

#include <cstdint>

#include "charm/contracts/config_transport_types.hpp"
#include "charm/ports/config_store_port.hpp"

namespace charm::app {

class ConfigTransportService {
 public:
  static constexpr std::uint32_t kProtocolVersion = 1;
  static constexpr std::uint32_t kExpectedIntegrity = 0x43464731;  // "CFG1"

  explicit ConfigTransportService(charm::ports::ConfigStorePort& config_store);

  charm::contracts::ConfigTransportResponse HandleRequest(
      const charm::contracts::ConfigTransportRequest& request) const;

 private:
  charm::contracts::ConfigTransportResponse Reject(
      const charm::contracts::ConfigTransportRequest& request,
      charm::contracts::ErrorCategory category, std::uint32_t reason) const;
  bool IsEnvelopeValid(const charm::contracts::ConfigTransportRequest& request,
                       charm::contracts::ConfigTransportResponse* rejection) const;

  charm::ports::ConfigStorePort& config_store_;
};

}  // namespace charm::app
