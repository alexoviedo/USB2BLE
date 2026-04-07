#pragma once

#include "charm/core/mapping_bundle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/ports/config_store_port.hpp"

namespace charm::app {

// Attempts to load persisted configuration from the given store.
// If valid, activates the compiled mapping bundle and selects the profile.
void ActivatePersistedConfig(charm::ports::ConfigStorePort& store,
                             charm::core::MappingBundleLoader& mapping_bundle_loader,
                             charm::core::Supervisor& supervisor);

}  // namespace charm::app
