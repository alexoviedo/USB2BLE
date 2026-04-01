#pragma once

#include "charm/ports/config_store_port.hpp"

namespace charm::platform {

class ConfigStoreNvs : public charm::ports::ConfigStorePort {
 public:
  ConfigStoreNvs() = default;
  ~ConfigStoreNvs() override;

  charm::contracts::LoadConfigResult LoadConfig(const charm::contracts::LoadConfigRequest& request) override;
  charm::contracts::PersistConfigResult PersistConfig(const charm::contracts::PersistConfigRequest& request) override;
  charm::ports::ClearConfigResult ClearConfig(const charm::ports::ClearConfigRequest& request) override;
  charm::ports::PersistedConfigRecord PeekPersistedConfig() const override;

 private:
  charm::ports::PersistedConfigRecord cached_config_{};
};

}  // namespace charm::platform
