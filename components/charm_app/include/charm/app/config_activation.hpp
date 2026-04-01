#pragma once

#include "charm/core/supervisor.hpp"
#include "charm/ports/config_store_port.hpp"

namespace charm::app {

// Attempts to load persisted configuration from the given store.
// If valid, activates the mapping bundle and selects the profile in the supervisor.
void ActivatePersistedConfig(charm::ports::ConfigStorePort& store, charm::core::Supervisor& supervisor);

}  // namespace charm::app
