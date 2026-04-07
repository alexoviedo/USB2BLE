#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/events.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::core {

inline constexpr std::size_t kMaxMappingEntries = 256;
inline constexpr std::int32_t kMappingScaleOne = 1024;

enum class LogicalElementType : std::uint8_t {
  kUnknown = 0,
  kAxis = 1,
  kButton = 2,
  kTrigger = 3,
  kHat = 4,
};

struct LogicalElementRef {
  LogicalElementType type{LogicalElementType::kUnknown};
  std::uint16_t index{0};
};

struct MappingEntry {
  charm::contracts::ElementKeyHash source{};
  charm::contracts::InputElementType source_type{charm::contracts::InputElementType::kUnknown};
  LogicalElementRef target{};
  std::int32_t scale{kMappingScaleOne};
  std::int32_t offset{0};
  std::int32_t deadzone{0};
  std::int32_t clamp_min{-127};
  std::int32_t clamp_max{127};
};

struct CompiledMappingBundle {
  charm::contracts::MappingBundleRef bundle_ref{};
  std::array<MappingEntry, kMaxMappingEntries> entries{};
  std::size_t entry_count{0};
};

struct MappingConfigDocument {
  const std::uint8_t* bytes{nullptr};
  std::size_t size{0};
};

inline constexpr charm::contracts::BundleVersion kSupportedMappingBundleVersion = 1;

struct ValidateMappingBundleRequest {
  const CompiledMappingBundle* bundle{nullptr};
};

struct ValidateMappingBundleResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

class MappingBundleValidator {
 public:
  virtual ~MappingBundleValidator() = default;

  virtual ValidateMappingBundleResult Validate(const ValidateMappingBundleRequest& request) const = 0;
};

class DefaultMappingBundleValidator : public MappingBundleValidator {
 public:
  ValidateMappingBundleResult Validate(const ValidateMappingBundleRequest& request) const override;
};

std::uint32_t ComputeMappingBundleHash(const CompiledMappingBundle& bundle);

struct LoadMappingBundleRequest {
  const CompiledMappingBundle* bundle{nullptr};
};

struct LoadMappingBundleResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

struct GetActiveBundleRequest {};

struct GetActiveBundleResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  const CompiledMappingBundle* bundle{nullptr};
};

class MappingBundleLoader {
 public:
  virtual ~MappingBundleLoader() = default;

  virtual LoadMappingBundleResult Load(const LoadMappingBundleRequest& request) = 0;
  virtual GetActiveBundleResult GetActiveBundle(const GetActiveBundleRequest& request) const = 0;
  virtual void ClearActiveBundle() = 0;
};

class DefaultMappingBundleLoader : public MappingBundleLoader {
 public:
  explicit DefaultMappingBundleLoader(const MappingBundleValidator* validator);

  LoadMappingBundleResult Load(const LoadMappingBundleRequest& request) override;
  GetActiveBundleResult GetActiveBundle(const GetActiveBundleRequest& request) const override;
  void ClearActiveBundle() override;

 private:
  const MappingBundleValidator* validator_;
  mutable std::mutex mutex_{};
  std::shared_ptr<const CompiledMappingBundle> active_bundle_{};
};

}  // namespace charm::core
