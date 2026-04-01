#include "charm/app/config_activation.hpp"

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::app {

void ActivatePersistedConfig(charm::ports::ConfigStorePort& store, charm::core::Supervisor& supervisor) {
  charm::contracts::LoadConfigRequest load_request{};
  auto load_result = store.LoadConfig(load_request);

  if (load_result.status == charm::contracts::ContractStatus::kOk) {
    charm::contracts::ActivateMappingBundleRequest bundle_request{};
    bundle_request.mapping_bundle = load_result.mapping_bundle;
    supervisor.ActivateMappingBundle(bundle_request);

    charm::contracts::SelectProfileRequest profile_request{};
    profile_request.profile_id = load_result.profile_id;
    supervisor.SelectProfile(profile_request);
  }
}

}  // namespace charm::app
