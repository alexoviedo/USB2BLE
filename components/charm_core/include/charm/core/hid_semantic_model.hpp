#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/contracts/transport_types.hpp"

namespace charm::core {

inline constexpr std::size_t kMaxCollectionsPerInterface = 64;
inline constexpr std::size_t kMaxFieldsPerInterface = 256;

enum class CollectionKind : std::uint8_t {
  kUnknown = 0,
  kApplication = 1,
  kPhysical = 2,
  kLogical = 3,
};

struct CollectionDescriptor {
  charm::contracts::UsagePage usage_page{0};
  charm::contracts::Usage usage{0};
  charm::contracts::CollectionIndex collection_index{0};
  CollectionKind kind{CollectionKind::kUnknown};
};

struct FieldDescriptor {
  charm::contracts::ReportId report_id{0};
  charm::contracts::UsagePage usage_page{0};
  charm::contracts::Usage usage{0};
  charm::contracts::CollectionIndex collection_index{0};
  charm::contracts::LogicalIndex logical_index{0};
  std::uint16_t bit_offset{0};
  std::uint16_t bit_size{0};
  bool is_signed{false};
};

struct SemanticDescriptorModel {
  std::array<CollectionDescriptor, kMaxCollectionsPerInterface> collections{};
  std::size_t collection_count{0};
  std::array<FieldDescriptor, kMaxFieldsPerInterface> fields{};
  std::size_t field_count{0};
};

struct ParseDescriptorRequest {
  charm::contracts::DeviceHandle device_handle{};
  charm::contracts::InterfaceHandle interface_handle{};
  charm::contracts::InterfaceNumber interface_number{0};
  charm::contracts::RawDescriptorRef descriptor{};
};

struct ParseDescriptorResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  SemanticDescriptorModel semantic_model{};
};

class HidDescriptorParser {
 public:
  virtual ~HidDescriptorParser() = default;

  virtual ParseDescriptorResult ParseDescriptor(const ParseDescriptorRequest& request) = 0;
};

class DefaultHidDescriptorParser final : public HidDescriptorParser {
 public:
  ParseDescriptorResult ParseDescriptor(const ParseDescriptorRequest& request) override;
};

}  // namespace charm::core
