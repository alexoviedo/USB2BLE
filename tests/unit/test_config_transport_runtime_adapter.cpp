#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "charm/app/config_transport_runtime_adapter.hpp"
#include "charm/app/config_transport_service.hpp"
#include "charm/core/config_compiler.hpp"
#include "charm/core/mapping_bundle.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/test_support/fake_config_store_port.hpp"

namespace {

constexpr char kPersistFrame[] =
    "@CFG:{\"protocol_version\":2,\"request_id\":8,\"command\":\"config.persist\","
    "\"payload\":{\"mapping_document\":{\"version\":1,\"global\":{\"scale\":1.0,"
    "\"deadzone\":0.08,\"clamp_min\":-1.0,\"clamp_max\":1.0},\"axes\":[{"
    "\"target\":\"move_x\",\"source_index\":0,\"scale\":1.0,\"deadzone\":0.08,"
    "\"invert\":false}],\"buttons\":[{\"target\":\"action_a\",\"source_index\":0}]},"
    "\"profile_id\":1,\"bonding_material\":[1,2,3]},\"integrity\":\"CFG1\"}\n";

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

struct ConfigRuntimeHarness {
  ConfigRuntimeHarness()
      : loader(&validator),
        service(config_store, compiler, loader, supervisor),
        adapter(service) {}

  charm::test_support::FakeConfigStorePort config_store;
  charm::core::DefaultConfigCompiler compiler;
  charm::core::DefaultMappingBundleValidator validator;
  charm::core::DefaultMappingBundleLoader loader;
  charm::core::DefaultSupervisor supervisor;
  charm::app::ConfigTransportService service;
  charm::app::ConfigTransportRuntimeAdapter adapter;
};

}  // namespace

TEST(ConfigTransportRuntimeAdapterTest, HandlesValidGetCapabilitiesCommand) {
  ConfigRuntimeHarness harness;

  const std::string frame =
      "@CFG:{\"protocol_version\":2,\"request_id\":7,\"command\":\"config.get_capabilities\",\"payload\":{},\"integrity\":\"CFG1\"}\n";

  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                               frame.size());

  ASSERT_TRUE(harness.adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_EQ(response.rfind("@CFG:", 0), 0u);
  EXPECT_NE(response.find("\"status\":\"kOk\""), std::string::npos);
  EXPECT_NE(response.find("\"protocol_version\":2"), std::string::npos);
  EXPECT_NE(response.find("\"supports_persist\":true"), std::string::npos);
  EXPECT_FALSE(harness.adapter.HasPendingFrame());
}

TEST(ConfigTransportRuntimeAdapterTest, TracksParsedAndEmittedFrameCounts) {
  ConfigRuntimeHarness harness;

  const auto initial = harness.adapter.Stats();
  EXPECT_EQ(initial.parsed_frames, 0u);
  EXPECT_EQ(initial.emitted_frames, 0u);

  const std::string noise = "I (12) boot: hello world\n";
  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(noise.data()),
                               noise.size());
  const auto after_noise = harness.adapter.Stats();
  EXPECT_EQ(after_noise.parsed_frames, 0u);
  EXPECT_EQ(after_noise.emitted_frames, 0u);

  const std::string frame =
      "@CFG:{\"protocol_version\":2,\"request_id\":7,\"command\":\"config.get_capabilities\",\"payload\":{},\"integrity\":\"CFG1\"}\n";

  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                               frame.size());
  const auto after_parse = harness.adapter.Stats();
  EXPECT_EQ(after_parse.parsed_frames, 1u);
  EXPECT_EQ(after_parse.emitted_frames, 0u);

  (void)ReadSingleFrame(harness.adapter);
  const auto after_emit = harness.adapter.Stats();
  EXPECT_EQ(after_emit.parsed_frames, 1u);
  EXPECT_EQ(after_emit.emitted_frames, 1u);
}

TEST(ConfigTransportRuntimeAdapterTest, RejectsMalformedFrameDeterministically) {
  ConfigRuntimeHarness harness;

  const std::string frame =
      "@CFG:{\"request_id\":7,\"command\":\"config.get_capabilities\",\"integrity\":\"CFG1\"}\n";

  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(frame.data()),
                               frame.size());

  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_NE(response.find("\"status\":\"kRejected\""), std::string::npos);
  EXPECT_NE(response.find("\"reason\":101"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, PropagatesServiceStatusAndFaultLosslessly) {
  ConfigRuntimeHarness harness;
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kFailed;
  persist_result.fault_code.category =
      charm::contracts::ErrorCategory::kPersistenceFailure;
  persist_result.fault_code.reason = 42;
  harness.config_store.SetPersistConfigResult(persist_result);

  harness.adapter.ConsumeBytes(
      reinterpret_cast<const std::uint8_t*>(kPersistFrame), sizeof(kPersistFrame) - 1);

  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_NE(response.find("\"status\":\"kFailed\""), std::string::npos);
  EXPECT_NE(response.find("\"category\":\"kPersistenceFailure\""),
            std::string::npos);
  EXPECT_NE(response.find("\"reason\":42"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, SupportsFramingAcrossChunkBoundaries) {
  ConfigRuntimeHarness harness;
  charm::ports::ClearConfigResult clear_result{};
  clear_result.status = charm::contracts::ContractStatus::kOk;
  harness.config_store.SetClearConfigResult(clear_result);

  const std::string first_half =
      "@CFG:{\"protocol_version\":2,\"request_id\":9,\"command\":\"config.clear\"";
  const std::string second_half = ",\"payload\":{},\"integrity\":\"CFG1\"}\n";

  harness.adapter.ConsumeBytes(
      reinterpret_cast<const std::uint8_t*>(first_half.data()), first_half.size());
  EXPECT_FALSE(harness.adapter.HasPendingFrame());

  harness.adapter.ConsumeBytes(
      reinterpret_cast<const std::uint8_t*>(second_half.data()), second_half.size());
  EXPECT_TRUE(harness.adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_NE(response.find("\"status\":\"kOk\""), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, RejectsOversizedFrameDeterministically) {
  ConfigRuntimeHarness harness;

  std::string oversized(3000, 'a');
  oversized.insert(0, "@CFG:");
  oversized.push_back('\n');

  harness.adapter.ConsumeBytes(
      reinterpret_cast<const std::uint8_t*>(oversized.data()), oversized.size());

  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_NE(response.find("\"status\":\"kRejected\""), std::string::npos);
  EXPECT_NE(response.find("\"category\":\"kCapacityExceeded\""),
            std::string::npos);
  EXPECT_NE(response.find("\"reason\":100"), std::string::npos);
}

TEST(ConfigTransportRuntimeAdapterTest, IgnoresNonControlPrefixedLines) {
  ConfigRuntimeHarness harness;

  const std::string noise = "I (12) boot: hello world\n";
  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(noise.data()),
                               noise.size());
  EXPECT_FALSE(harness.adapter.HasPendingFrame());
}

TEST(ConfigTransportRuntimeAdapterTest, ProcessesOnlyCfgPrefixedFramesInMixedStreams) {
  ConfigRuntimeHarness harness;

  const std::string mixed =
      "I (12) boot: hello\n"
      "@CFG:{\"protocol_version\":2,\"request_id\":10,\"command\":\"config.get_capabilities\",\"payload\":{},\"integrity\":\"CFG1\"}\n"
      "W (13) telemetry: still running\n";

  harness.adapter.ConsumeBytes(reinterpret_cast<const std::uint8_t*>(mixed.data()),
                               mixed.size());

  ASSERT_TRUE(harness.adapter.HasPendingFrame());
  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_EQ(response.rfind("@CFG:", 0), 0u);
  EXPECT_NE(response.find("\"request_id\":10"), std::string::npos);
  EXPECT_FALSE(harness.adapter.HasPendingFrame());
}

TEST(ConfigTransportRuntimeAdapterTest, PersistFrameCompilesAndReturnsBundleMetadata) {
  ConfigRuntimeHarness harness;
  charm::contracts::PersistConfigResult persist_result{};
  persist_result.status = charm::contracts::ContractStatus::kOk;
  harness.config_store.SetPersistConfigResult(persist_result);

  harness.adapter.ConsumeBytes(
      reinterpret_cast<const std::uint8_t*>(kPersistFrame), sizeof(kPersistFrame) - 1);

  const auto response = ReadSingleFrame(harness.adapter);
  EXPECT_NE(response.find("\"status\":\"kOk\""), std::string::npos);
  EXPECT_NE(response.find("\"bundle_id\":"), std::string::npos);
  EXPECT_NE(response.find("\"profile_id\":1"), std::string::npos);
  EXPECT_EQ(harness.config_store.PersistCallCount(), 1);
  EXPECT_EQ(harness.config_store.LastPersistRequest().compiled_mapping_bundle_size,
            sizeof(charm::core::CompiledMappingBundle));
}
