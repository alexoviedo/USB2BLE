#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

#include "charm/core/config_compiler.hpp"

namespace charm::core::test {

namespace {

constexpr std::string_view kValidDocument = R"json({
  "version": 1,
  "global": {
    "scale": 0.5,
    "deadzone": 0.1,
    "clamp_min": -0.5,
    "clamp_max": 0.75
  },
  "axes": [
    {
      "target": "move_x",
      "source_index": 0,
      "scale": 2.0,
      "deadzone": 0.2,
      "invert": true
    }
  ],
  "buttons": [
    {
      "target": "action_a",
      "source_index": 1
    }
  ]
})json";

constexpr std::string_view kOutOfOrderDocument = R"json({
  "version": 1,
  "global": {
    "scale": 1.0,
    "deadzone": 0.0,
    "clamp_min": -1.0,
    "clamp_max": 1.0
  },
  "axes": [
    {
      "target": "move_y",
      "source_index": 1,
      "scale": 1.0,
      "deadzone": 0.0,
      "invert": false
    },
    {
      "target": "move_x",
      "source_index": 0,
      "scale": 1.0,
      "deadzone": 0.0,
      "invert": false
    }
  ],
  "buttons": []
})json";

MappingConfigDocument MakeDocument(std::string_view text) {
  return {
      .bytes = reinterpret_cast<const std::uint8_t*>(text.data()),
      .size = text.size(),
  };
}

}  // namespace

TEST(ConfigCompilerTest, CompilesValidDocumentDeterministically) {
  DefaultConfigCompiler compiler;

  const auto first = compiler.CompileConfig({.document = MakeDocument(kValidDocument)});
  const auto second = compiler.CompileConfig({.document = MakeDocument(kValidDocument)});

  ASSERT_EQ(first.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(second.status, charm::contracts::ContractStatus::kOk);
  EXPECT_EQ(std::memcmp(&first.bundle, &second.bundle, sizeof(first.bundle)), 0);
  EXPECT_EQ(first.bundle.bundle_ref.integrity, ComputeMappingBundleHash(first.bundle));
}

TEST(ConfigCompilerTest, CompilesSortedBundleDeterministicallyWhenInputOrderDiffers) {
  DefaultConfigCompiler compiler;

  const auto first = compiler.CompileConfig({.document = MakeDocument(kOutOfOrderDocument)});
  const auto second = compiler.CompileConfig({.document = MakeDocument(kOutOfOrderDocument)});

  ASSERT_EQ(first.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(second.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(first.bundle.entry_count, 2u);
  EXPECT_EQ(std::memcmp(&first.bundle, &second.bundle, sizeof(first.bundle)), 0);
  EXPECT_LT(first.bundle.entries[0].source.value, first.bundle.entries[1].source.value);
}

TEST(ConfigCompilerTest, CompiledBundleCarriesExpectedTransforms) {
  DefaultConfigCompiler compiler;
  const auto result = compiler.CompileConfig({.document = MakeDocument(kValidDocument)});

  ASSERT_EQ(result.status, charm::contracts::ContractStatus::kOk);
  ASSERT_EQ(result.bundle.entry_count, 2u);

  const MappingEntry* axis_entry = nullptr;
  const MappingEntry* button_entry = nullptr;
  for (std::size_t i = 0; i < result.bundle.entry_count; ++i) {
    const auto& entry = result.bundle.entries[i];
    if (entry.target.type == LogicalElementType::kAxis) {
      axis_entry = &entry;
    }
    if (entry.target.type == LogicalElementType::kButton) {
      button_entry = &entry;
    }
  }

  ASSERT_NE(axis_entry, nullptr);
  ASSERT_NE(button_entry, nullptr);

  EXPECT_EQ(axis_entry->source_type, charm::contracts::InputElementType::kAxis);
  EXPECT_EQ(axis_entry->target.type, LogicalElementType::kAxis);
  EXPECT_EQ(axis_entry->target.index, 0u);
  EXPECT_EQ(axis_entry->scale, -kMappingScaleOne);
  EXPECT_EQ(axis_entry->deadzone, 25);
  EXPECT_EQ(axis_entry->clamp_min, -64);
  EXPECT_EQ(axis_entry->clamp_max, 95);

  EXPECT_EQ(button_entry->source_type, charm::contracts::InputElementType::kButton);
  EXPECT_EQ(button_entry->target.type, LogicalElementType::kButton);
  EXPECT_EQ(button_entry->target.index, 0u);
  EXPECT_EQ(button_entry->scale, kMappingScaleOne);
  EXPECT_EQ(button_entry->clamp_min, 0);
  EXPECT_EQ(button_entry->clamp_max, 1);
}

TEST(ConfigCompilerTest, RejectsEmptyDocument) {
  DefaultConfigCompiler compiler;
  const auto result = compiler.ValidateConfig({});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kInvalidRequest);
  EXPECT_EQ(result.fault_code.reason, 1u);
}

TEST(ConfigCompilerTest, RejectsInvalidClampRange) {
  constexpr std::string_view kInvalidDocument = R"json({
    "version": 1,
    "global": {
      "scale": 1.0,
      "deadzone": 0.1,
      "clamp_min": 0.9,
      "clamp_max": 0.1
    },
    "axes": [],
    "buttons": []
  })json";

  DefaultConfigCompiler compiler;
  const auto result =
      compiler.ValidateConfig({.document = MakeDocument(kInvalidDocument)});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, charm::contracts::ErrorCategory::kInvalidRequest);
  EXPECT_EQ(result.fault_code.reason, 5u);
}

TEST(ConfigCompilerTest, RejectsUnknownTargets) {
  constexpr std::string_view kInvalidDocument = R"json({
    "version": 1,
    "global": {
      "scale": 1.0,
      "deadzone": 0.0,
      "clamp_min": -1.0,
      "clamp_max": 1.0
    },
    "axes": [
      {
        "target": "unknown_axis",
        "source_index": 0,
        "scale": 1.0,
        "deadzone": 0.0,
        "invert": false
      }
    ],
    "buttons": []
  })json";

  DefaultConfigCompiler compiler;
  const auto result =
      compiler.CompileConfig({.document = MakeDocument(kInvalidDocument)});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kConfigurationRejected);
  EXPECT_EQ(result.fault_code.reason, 6u);
}

TEST(ConfigCompilerTest, RejectsDuplicateSourcesWithinFamily) {
  constexpr std::string_view kInvalidDocument = R"json({
    "version": 1,
    "global": {
      "scale": 1.0,
      "deadzone": 0.0,
      "clamp_min": -1.0,
      "clamp_max": 1.0
    },
    "axes": [
      {
        "target": "move_x",
        "source_index": 0,
        "scale": 1.0,
        "deadzone": 0.0,
        "invert": false
      },
      {
        "target": "move_y",
        "source_index": 0,
        "scale": 1.0,
        "deadzone": 0.0,
        "invert": false
      }
    ],
    "buttons": []
  })json";

  DefaultConfigCompiler compiler;
  const auto result =
      compiler.CompileConfig({.document = MakeDocument(kInvalidDocument)});

  EXPECT_EQ(result.status, charm::contracts::ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category,
            charm::contracts::ErrorCategory::kConfigurationRejected);
  EXPECT_EQ(result.fault_code.reason, 10u);
}

}  // namespace charm::core::test
