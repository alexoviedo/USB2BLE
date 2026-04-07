#include "charm/app/config_activation.hpp"

#include <cstring>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::app {

void ActivatePersistedConfig(charm::ports::ConfigStorePort& store,
                             charm::core::MappingBundleLoader& mapping_bundle_loader,
                             charm::core::Supervisor& supervisor) {
  charm::contracts::LoadConfigRequest load_request{};
  auto load_result = store.LoadConfig(load_request);

  if (load_result.status != charm::contracts::ContractStatus::kOk) {
    return;
  }

  const bool has_compiled_bundle_blob =
      load_result.compiled_mapping_bundle != nullptr ||
      load_result.compiled_mapping_bundle_size != 0;
  if (has_compiled_bundle_blob) {
    if (load_result.compiled_mapping_bundle == nullptr ||
        load_result.compiled_mapping_bundle_size !=
            sizeof(charm::core::CompiledMappingBundle)) {
      return;
    }

    charm::core::CompiledMappingBundle bundle{};
    std::memcpy(&bundle, load_result.compiled_mapping_bundle, sizeof(bundle));
    const auto load_bundle_result =
        mapping_bundle_loader.Load({.bundle = &bundle});
    if (load_bundle_result.status != charm::contracts::ContractStatus::kOk) {
      return;
    }
  }

  charm::contracts::ActivateMappingBundleRequest bundle_request{};
  bundle_request.mapping_bundle = load_result.mapping_bundle;
  supervisor.ActivateMappingBundle(bundle_request);

  charm::contracts::SelectProfileRequest profile_request{};
  profile_request.profile_id = load_result.profile_id;
  supervisor.SelectProfile(profile_request);
}

}  // namespace charm::app
