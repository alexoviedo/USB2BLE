#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "charm/app/config_transport_runtime_adapter.hpp"
#include "charm/app/config_transport_service.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace {

class CapturingWriter final : public charm::app::SerialTransportWriter {
 public:
  bool Write(const std::uint8_t* bytes, std::size_t size) override {
    writes.emplace_back(reinterpret_cast<const char*>(bytes), size);
    return true;
  }

  std::vector<std::string> writes{};
};

std::string ReadSingleFrame(charm::app::ConfigTransportRuntimeAdapter& adapter) {
  CapturingWriter writer;
  EXPECT_TRUE(adapter.WritePendingFrame(writer));
  EXPECT_EQ(writer.writes.size(), 1u);
  return writer.writes[0];
}

}  // namespace

TEST(ConfigTransportRuntimeAdapterTest, HandlesValidGetCapabilitiesCommand) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string frame =
      "@CFG:{\"protocol_version\":1,\"request_id\":7,\"command\":\"config.get_capabilities\",\"payload\":{},\"integrity\":\"CFG1\"}\n";

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                       frame.size());

  ASSERT_TRUE(adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(adapter);
  EXPECT_EQ(response.rfind("@CFG:", 0), 0u);
  EXPECT_NE(response.find("\"status\":\"kOk\""), std::string::npos);
  EXPECT_NE(response.find("\"protocol_version\":1"), std::string::npos);
  EXPECT_NE(response.find("\"supports_persist\":true"), std::string::npos);
  EXPECT_FALSE(adapter.HasPendingFrame());
}

TEST(ConfigTransportRuntimeAdapterTest, RejectsMalformedFrameDeterministically) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string frame =
      "@CFG:{\"request_id\":7,\"command\":\"config.get_capabilities\",\"integrity\":\"CFG1\"}\n";

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                       frame.size());

  const auto response = ReadSingleFrame(adapter);
  EXPECT_NE(response.find("\"status\":\"kRejected\""), std::string::npos);
  EXPECT_NE(response.find("\"reason\":101"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, PropagatesServiceStatusAndFaultLosslessly) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kFailed;
  persist_result.fault_code.category =
      charm::contracts::ErrorCategory::kPersistenceFailure;
  persist_result.fault_code.reason = 42;
  config_store.SetPersistConfigResult(persist_result);

  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string frame =
      "@CFG:{\"protocol_version\":1,\"request_id\":8,\"command\":\"config.persist\","
      "\"payload\":{\"mapping_bundle\":{\"bundle_id\":11,\"version\":1,\"integrity\":22},\"profile_id\":1,\"bonding_material\":[1,2,3]},"
      "\"integrity\":\"CFG1\"}\n";

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                       frame.size());

  const auto response = ReadSingleFrame(adapter);
  EXPECT_NE(response.find("\"status\":\"kFailed\""), std::string::npos);
  EXPECT_NE(response.find("\"category\":\"kPersistenceFailure\""),
            std::string::npos);
  EXPECT_NE(response.find("\"reason\":42"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, SupportsFramingAcrossChunkBoundaries) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::ports::ClearConfigResult clear_result{};
  clear_result.status = charm::contracts::ContractStatus::kOk;
  config_store.SetClearConfigResult(clear_result);

  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string first_half =
      "@CFG:{\"protocol_version\":1,\"request_id\":9,\"command\":\"config.clear\"";
  const std::string second_half = ",\"payload\":{},\"integrity\":\"CFG1\"}\n";

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(first_half.data()),
                       first_half.size());
  EXPECT_FALSE(adapter.HasPendingFrame());

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(second_half.data()),
                       second_half.size());
  EXPECT_TRUE(adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(adapter);
  EXPECT_NE(response.find("\"status\":\"kOk\""), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, RejectsOversizedFrameDeterministically) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  std::string oversized(3000, 'a');
  oversized.insert(0, "@CFG:");
  oversized.push_back('\n');

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(oversized.data()),
                       oversized.size());

  const auto response = ReadSingleFrame(adapter);
  EXPECT_NE(response.find("\"status\":\"kRejected\""), std::string::npos);
  EXPECT_NE(response.find("\"category\":\"kCapacityExceeded\""),
            std::string::npos);
  EXPECT_NE(response.find("\"reason\":100"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, IgnoresNonControlPrefixedLines) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string noise = "I (12) boot: hello world\n";
  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(noise.data()),
                       noise.size());
  EXPECT_FALSE(adapter.HasPendingFrame());
}

TEST(ConfigTransportRuntimeAdapterTest, ProcessesOnlyCfgPrefixedFramesInMixedStreams) {
  charm::test_support::FakeConfigStorePort config_store;
  charm::app::ConfigTransportService service(config_store);
  charm::app::ConfigTransportRuntimeAdapter adapter(service);

  const std::string mixed =
      "I (12) boot: hello\n"
      "@CFG:{\"protocol_version\":1,\"request_id\":10,\"command\":\"config.get_capabilities\",\"payload\":{},\"integrity\":\"CFG1\"}\n"
      "W (13) telemetry: still running\n";

  adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(mixed.data()),
                       mixed.size());

  ASSERT_TRUE(adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(adapter);
  EXPECT_EQ(response.rfind("@CFG:", 0), 0u);
  EXPECT_NE(response.find("\"request_id\":10"), std::string::npos);
  EXPECT_FALSE(adapter.HasPendingFrame());
}
