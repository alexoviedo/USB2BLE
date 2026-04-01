#include "charm/app/startup_storage_lifecycle.hpp"

#include <nvs_flash.h>

#include "charm/contracts/error_types.hpp"

namespace charm::app {

namespace {

#ifndef ESP_OK
constexpr int kEspOk = 0;
#else
constexpr int kEspOk = ESP_OK;
#endif

#ifndef ESP_ERR_NVS_NO_FREE_PAGES
constexpr int kErrNvsNoFreePages = 0x110d;
#else
constexpr int kErrNvsNoFreePages = ESP_ERR_NVS_NO_FREE_PAGES;
#endif

#ifndef ESP_ERR_NVS_NEW_VERSION_FOUND
constexpr int kErrNvsNewVersionFound = 0x1110;
#else
constexpr int kErrNvsNewVersionFound = ESP_ERR_NVS_NEW_VERSION_FOUND;
#endif

constexpr std::uint32_t kReasonInitFailed = 1;
constexpr std::uint32_t kReasonEraseFailed = 2;
constexpr std::uint32_t kReasonReinitFailed = 3;

bool IsRecoverableInitFailure(int code) {
  return code == kErrNvsNoFreePages || code == kErrNvsNewVersionFound;
}

}  // namespace

StorageInitFns DefaultStorageInitFns() {
  return StorageInitFns{
      .init = &nvs_flash_init,
      .erase = &nvs_flash_erase,
  };
}

StorageInitOutcome InitializeStorage(const StorageInitFns& fns) {
  StorageInitOutcome outcome{};
  if (fns.init == nullptr || fns.erase == nullptr) {
    outcome.reason = kReasonInitFailed;
    return outcome;
  }

  const int init_code = fns.init();
  if (init_code == kEspOk) {
    outcome.ok = true;
    return outcome;
  }

  if (!IsRecoverableInitFailure(init_code)) {
    outcome.reason = kReasonInitFailed;
    return outcome;
  }

  if (fns.erase() != kEspOk) {
    outcome.reason = kReasonEraseFailed;
    return outcome;
  }

  if (fns.init() != kEspOk) {
    outcome.reason = kReasonReinitFailed;
    return outcome;
  }

  outcome.ok = true;
  outcome.recovered = true;
  return outcome;
}

bool InitializeStorageAndActivate(
    charm::ports::ConfigStorePort& store, charm::core::Supervisor& supervisor,
    void (*activate_fn)(charm::ports::ConfigStorePort&, charm::core::Supervisor&),
    const StorageInitFns& fns) {
  const StorageInitOutcome outcome = InitializeStorage(fns);
  if (!outcome.ok) {
    charm::contracts::FaultRecordRef fault{};
    fault.fault_code.category = charm::contracts::ErrorCategory::kPersistenceFailure;
    fault.fault_code.reason = outcome.reason;
    fault.severity = charm::contracts::FaultSeverity::kFatal;
    supervisor.SetLastFault(fault);
    return false;
  }

  if (activate_fn != nullptr) {
    activate_fn(store, supervisor);
  }
  return true;
}

}  // namespace charm::app
