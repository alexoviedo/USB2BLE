#pragma once

#include <cstdint>

#include "charm/contracts/config_transport_types.hpp"
#include "charm/core/config_compiler.hpp"
#include "charm/core/mapping_bundle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/ports/config_store_port.hpp"

namespace charm::app {

class ConfigTransportService {
 public:
  static constexpr std::uint32_t kProtocolVersion = 2;
  static constexpr std::uint32_t kExpectedIntegrity = 0x43464731;  // "CFG1"

  ConfigTransportService(charm::ports::ConfigStorePort& config_store,
                         charm::core::ConfigCompiler& config_compiler,
                         charm::core::MappingBundleLoader& mapping_bundle_loader,
                         charm::core::Supervisor& supervisor);

  charm::contracts::ConfigTransportResponse HandleRequest(
      const charm::contracts::ConfigTransportRequest& request);

 private:
  charm::contracts::ConfigTransportResponse Reject(
      const charm::contracts::ConfigTransportRequest& request,
      charm::contracts::ErrorCategory category, std::uint32_t reason) const;
  bool IsEnvelopeValid(const charm::contracts::ConfigTransportRequest& request,
                       charm::contracts::ConfigTransportResponse* rejection) const;

  charm::ports::ConfigStorePort& config_store_;
  charm::core::ConfigCompiler& config_compiler_;
  charm::core::MappingBundleLoader& mapping_bundle_loader_;
  charm::core::Supervisor& supervisor_;
};

}  // namespace charm::app
