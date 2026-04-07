#include "charm/app/config_transport_service.hpp"

namespace charm::app {

namespace {
constexpr std::uint32_t kReasonUnsupportedProtocol = 1;
constexpr std::uint32_t kReasonMissingRequestId = 2;
constexpr std::uint32_t kReasonIntegrityMismatch = 3;
constexpr std::uint32_t kReasonPersistPayloadInvalid = 4;
constexpr std::uint32_t kReasonCompiledBundleActivationFailed = 5;
}  // namespace

ConfigTransportService::ConfigTransportService(
    charm::ports::ConfigStorePort& config_store,
    charm::core::ConfigCompiler& config_compiler,
    charm::core::MappingBundleLoader& mapping_bundle_loader,
    charm::core::Supervisor& supervisor)
    : config_store_(config_store),
      config_compiler_(config_compiler),
      mapping_bundle_loader_(mapping_bundle_loader),
      supervisor_(supervisor) {}

charm::contracts::ConfigTransportResponse ConfigTransportService::HandleRequest(
    const charm::contracts::ConfigTransportRequest& request) {
  charm::contracts::ConfigTransportResponse rejection{};
  if (!IsEnvelopeValid(request, &rejection)) {
    return rejection;
  }

  charm::contracts::ConfigTransportResponse response{};
  response.protocol_version = request.protocol_version;
  response.request_id = request.request_id;
  response.command = request.command;

  switch (request.command) {
    case charm::contracts::ConfigTransportCommand::kPersist: {
      if (request.mapping_document == nullptr || request.mapping_document_size == 0 ||
          request.profile_id.value == 0) {
        return Reject(request, charm::contracts::ErrorCategory::kInvalidRequest,
                      kReasonPersistPayloadInvalid);
      }

      const auto compile_result = config_compiler_.CompileConfig(
          charm::core::CompileConfigRequest{
              .document =
                  charm::core::MappingConfigDocument{
                      .bytes = request.mapping_document,
                      .size = request.mapping_document_size,
                  },
          });
      if (compile_result.status != charm::contracts::ContractStatus::kOk) {
        response.status = compile_result.status;
        response.fault_code = compile_result.fault_code;
        return response;
      }

      charm::contracts::PersistConfigRequest persist_request{};
      persist_request.mapping_bundle = compile_result.bundle.bundle_ref;
      persist_request.compiled_mapping_bundle =
          reinterpret_cast<const std::uint8_t*>(&compile_result.bundle);
      persist_request.compiled_mapping_bundle_size =
          sizeof(compile_result.bundle);
      persist_request.profile_id = request.profile_id;
      persist_request.bonding_material = request.bonding_material;
      persist_request.bonding_material_size = request.bonding_material_size;
      const auto persist_result = config_store_.PersistConfig(persist_request);
      response.status = persist_result.status;
      response.fault_code = persist_result.fault_code;
      if (persist_result.status != charm::contracts::ContractStatus::kOk) {
        return response;
      }

      const auto load_bundle_result =
          mapping_bundle_loader_.Load({.bundle = &compile_result.bundle});
      if (load_bundle_result.status != charm::contracts::ContractStatus::kOk) {
        response.status = charm::contracts::ContractStatus::kFailed;
        response.fault_code.category =
            charm::contracts::ErrorCategory::kInvalidState;
        response.fault_code.reason = kReasonCompiledBundleActivationFailed;
        return response;
      }

      (void)supervisor_.ActivateMappingBundle(
          {.mapping_bundle = compile_result.bundle.bundle_ref});
      (void)supervisor_.SelectProfile({.profile_id = request.profile_id});
      response.mapping_bundle = compile_result.bundle.bundle_ref;
      response.profile_id = request.profile_id;
      return response;
    }
    case charm::contracts::ConfigTransportCommand::kLoad: {
      const auto load_result = config_store_.LoadConfig({});
      response.status = load_result.status;
      response.fault_code = load_result.fault_code;
      response.mapping_bundle = load_result.mapping_bundle;
      response.profile_id = load_result.profile_id;
      response.bonding_material = load_result.bonding_material;
      response.bonding_material_size = load_result.bonding_material_size;
      return response;
    }
    case charm::contracts::ConfigTransportCommand::kClear: {
      const auto clear_result = config_store_.ClearConfig({});
      response.status = clear_result.status;
      response.fault_code = clear_result.fault_code;
      if (clear_result.status == charm::contracts::ContractStatus::kOk) {
        mapping_bundle_loader_.ClearActiveBundle();
        (void)supervisor_.ActivateMappingBundle({});
        (void)supervisor_.SelectProfile({});
      }
      return response;
    }
    case charm::contracts::ConfigTransportCommand::kGetCapabilities: {
      response.status = charm::contracts::ContractStatus::kOk;
      response.capabilities.protocol_version = kProtocolVersion;
      response.capabilities.supports_persist = true;
      response.capabilities.supports_load = true;
      response.capabilities.supports_clear = true;
      response.capabilities.supports_get_capabilities = true;
      response.capabilities.supports_ble_transport = false;
      return response;
    }
  }

  return Reject(request, charm::contracts::ErrorCategory::kUnsupportedCapability, 5);
}

charm::contracts::ConfigTransportResponse ConfigTransportService::Reject(
    const charm::contracts::ConfigTransportRequest& request,
    charm::contracts::ErrorCategory category, std::uint32_t reason) const {
  charm::contracts::ConfigTransportResponse response{};
  response.protocol_version = request.protocol_version;
  response.request_id = request.request_id;
  response.command = request.command;
  response.status = charm::contracts::ContractStatus::kRejected;
  response.fault_code.category = category;
  response.fault_code.reason = reason;
  return response;
}

bool ConfigTransportService::IsEnvelopeValid(
    const charm::contracts::ConfigTransportRequest& request,
    charm::contracts::ConfigTransportResponse* rejection) const {
  if (request.protocol_version != kProtocolVersion) {
    *rejection = Reject(request, charm::contracts::ErrorCategory::kUnsupportedCapability,
                        kReasonUnsupportedProtocol);
    return false;
  }
  if (request.request_id == 0) {
    *rejection = Reject(request, charm::contracts::ErrorCategory::kInvalidRequest,
                        kReasonMissingRequestId);
    return false;
  }
  if (request.integrity != kExpectedIntegrity) {
    *rejection =
        Reject(request, charm::contracts::ErrorCategory::kIntegrityFailure, kReasonIntegrityMismatch);
    return false;
  }
  return true;
}

}  // namespace charm::app
