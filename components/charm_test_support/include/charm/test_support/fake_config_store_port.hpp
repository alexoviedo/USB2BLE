#pragma once

#include "charm/ports/config_store_port.hpp"

namespace charm::test_support {

class FakeConfigStorePort : public charm::ports::ConfigStorePort {
 public:
  void SetLoadConfigResult(charm::contracts::LoadConfigResult result) { load_result_ = result; }
  void SetPersistConfigResult(charm::contracts::PersistConfigResult result) { persist_result_ = result; }
  void SetClearConfigResult(charm::ports::ClearConfigResult result) { clear_result_ = result; }
  void SetPersistedConfig(charm::ports::PersistedConfigRecord record) { persisted_config_ = record; }
  int LoadCallCount() const { return load_calls_; }
  int PersistCallCount() const { return persist_calls_; }
  int ClearCallCount() const { return clear_calls_; }
  const charm::contracts::PersistConfigRequest& LastPersistRequest() const { return last_persist_request_; }

  charm::contracts::LoadConfigResult LoadConfig(const charm::contracts::LoadConfigRequest&) override {
    load_calls_++;
    return load_result_;
  }

  charm::contracts::PersistConfigResult PersistConfig(const charm::contracts::PersistConfigRequest& req) override {
    persist_calls_++;
    last_persist_request_ = req;
    if (persist_result_.status == charm::contracts::ContractStatus::kOk) {
      persisted_config_.mapping_bundle = req.mapping_bundle;
      persisted_config_.profile_id = req.profile_id;
      // We don't allocate or free in fake store for bonding material to keep it simple,
      // but we update the pointer/size if provided.
      persisted_config_.bonding_material = req.bonding_material;
      persisted_config_.bonding_material_size = req.bonding_material_size;
    }
    return persist_result_;
  }

  charm::ports::ClearConfigResult ClearConfig(const charm::ports::ClearConfigRequest&) override {
    clear_calls_++;
    return clear_result_;
  }

  charm::ports::PersistedConfigRecord PeekPersistedConfig() const override {
    return persisted_config_;
  }

 private:
  charm::contracts::LoadConfigResult load_result_{};
  charm::contracts::PersistConfigResult persist_result_{};
  charm::ports::ClearConfigResult clear_result_{};
  charm::ports::PersistedConfigRecord persisted_config_{};
  charm::contracts::PersistConfigRequest last_persist_request_{};
  int load_calls_{0};
  int persist_calls_{0};
  int clear_calls_{0};
};

}  // namespace charm::test_support
