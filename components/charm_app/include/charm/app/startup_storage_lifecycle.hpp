#pragma once

#include <cstdint>

#include "charm/core/supervisor.hpp"
#include "charm/ports/config_store_port.hpp"

namespace charm::app {

struct StorageInitFns {
  int (*init)();
  int (*erase)();
};

struct StorageInitOutcome {
  bool ok{false};
  bool recovered{false};
  std::uint32_t reason{0};
};

StorageInitFns DefaultStorageInitFns();
StorageInitOutcome InitializeStorage(const StorageInitFns& fns);
bool InitializeStorageAndActivate(
    charm::ports::ConfigStorePort& store, charm::core::Supervisor& supervisor,
    void (*activate_fn)(charm::ports::ConfigStorePort&, charm::core::Supervisor&),
    const StorageInitFns& fns);

}  // namespace charm::app
