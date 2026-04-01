#include "charm/core/hid_semantic_model.hpp"

#include <array>
#include <cstdint>

namespace charm::core {

namespace {

enum class ItemType : std::uint8_t { kMain = 0, kGlobal = 1, kLocal = 2, kReserved = 3 };

// Basic HID tag definitions
constexpr std::uint8_t kTagUsagePage = 0x0;
constexpr std::uint8_t kTagLogicalMin = 0x1;
constexpr std::uint8_t kTagLogicalMax = 0x2;
constexpr std::uint8_t kTagPhysicalMin = 0x3;
constexpr std::uint8_t kTagPhysicalMax = 0x4;
constexpr std::uint8_t kTagUnitExponent = 0x5;
constexpr std::uint8_t kTagUnit = 0x6;
constexpr std::uint8_t kTagReportSize = 0x7;
constexpr std::uint8_t kTagReportId = 0x8;
constexpr std::uint8_t kTagReportCount = 0x9;
constexpr std::uint8_t kTagPush = 0xA;
constexpr std::uint8_t kTagPop = 0xB;

constexpr std::uint8_t kTagUsage = 0x0;
constexpr std::uint8_t kTagUsageMin = 0x1;
constexpr std::uint8_t kTagUsageMax = 0x2;

constexpr std::uint8_t kTagInput = 0x8;
constexpr std::uint8_t kTagOutput = 0x9;
constexpr std::uint8_t kTagCollection = 0xA;
constexpr std::uint8_t kTagFeature = 0xB;
constexpr std::uint8_t kTagEndCollection = 0xC;

struct ParserState {
  charm::contracts::UsagePage usage_page{0};
  std::int32_t logical_min{0};
  std::int32_t logical_max{0};
  std::uint16_t report_size{0};
  std::uint16_t report_count{0};
  charm::contracts::ReportId report_id{0};
};

struct LocalState {
  std::array<charm::contracts::Usage, 32> usages{};
  std::size_t usage_count{0};
  charm::contracts::Usage usage_min{0};
  charm::contracts::Usage usage_max{0};
  bool has_usage_range{false};

  void Clear() {
    usage_count = 0;
    has_usage_range = false;
    usage_min = 0;
    usage_max = 0;
  }

  void AddUsage(charm::contracts::Usage usage) {
    if (usage_count < usages.size()) {
      usages[usage_count++] = usage;
    }
  }
};

std::int32_t SignExtend(std::uint32_t value, std::uint8_t size) {
  if (size == 1 && (value & 0x80)) {
    return static_cast<std::int32_t>(value | 0xFFFFFF00);
  } else if (size == 2 && (value & 0x8000)) {
    return static_cast<std::int32_t>(value | 0xFFFF0000);
  }
  return static_cast<std::int32_t>(value);
}

}  // namespace

ParseDescriptorResult DefaultHidDescriptorParser::ParseDescriptor(const ParseDescriptorRequest& request) {
  ParseDescriptorResult result;

  if (!request.descriptor.bytes || request.descriptor.size == 0) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kInvalidRequest,
        .reason = 1}; // Empty descriptor
    return result;
  }

  SemanticDescriptorModel model;
  ParserState global_state;
  LocalState local_state;

  std::uint16_t bit_offset_input = 0;
  std::uint16_t bit_offset_output = 0;
  std::uint16_t bit_offset_feature = 0;
  charm::contracts::ReportId current_report_id = 0;

  std::array<charm::contracts::CollectionIndex, kMaxCollectionsPerInterface> collection_stack{};
  std::size_t collection_depth = 0;
  charm::contracts::CollectionIndex next_collection_index = 1;

  const std::uint8_t* ptr = request.descriptor.bytes;
  const std::uint8_t* end = ptr + request.descriptor.size;

  while (ptr < end) {
    std::uint8_t header = *ptr++;
    if (header == 0xFE) { // Long item not supported by pure-core basic parser
      if (ptr < end) {
        std::uint8_t size = *ptr++;
        if (ptr < end) ptr++; // Skip data item tag
        if (ptr + size > end) {
          result.status = charm::contracts::ContractStatus::kRejected;
          result.fault_code = charm::contracts::FaultCode{
              .category = charm::contracts::ErrorCategory::kContractViolation,
              .reason = 2}; // Malformed descriptor
          return result;
        }
        ptr += size;
      }
      continue;
    }

    std::uint8_t size_code = header & 0x03;
    ItemType type = static_cast<ItemType>((header >> 2) & 0x03);
    std::uint8_t tag = (header >> 4) & 0x0F;

    std::uint8_t size = size_code;
    if (size_code == 3) size = 4;

    if (ptr + size > end) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kContractViolation,
          .reason = 2}; // Malformed descriptor
      return result;
    }

    std::uint32_t raw_value = 0;
    for (std::uint8_t i = 0; i < size; ++i) {
      raw_value |= (static_cast<std::uint32_t>(ptr[i]) << (i * 8));
    }
    ptr += size;

    if (type == ItemType::kGlobal) {
      switch (tag) {
        case kTagUsagePage:
          global_state.usage_page = static_cast<charm::contracts::UsagePage>(raw_value);
          break;
        case kTagLogicalMin:
          global_state.logical_min = SignExtend(raw_value, size);
          break;
        case kTagLogicalMax:
          global_state.logical_max = SignExtend(raw_value, size);
          break;
        case kTagReportSize:
          global_state.report_size = static_cast<std::uint16_t>(raw_value);
          break;
        case kTagReportCount:
          global_state.report_count = static_cast<std::uint16_t>(raw_value);
          break;
        case kTagReportId:
          global_state.report_id = static_cast<charm::contracts::ReportId>(raw_value);
          if (global_state.report_id != current_report_id) {
            current_report_id = global_state.report_id;
            bit_offset_input = 0; // Reset bit offset for a new report ID
            bit_offset_output = 0;
            bit_offset_feature = 0;
          }
          break;
        default:
          break;
      }
    } else if (type == ItemType::kLocal) {
      switch (tag) {
        case kTagUsage:
          local_state.AddUsage(static_cast<charm::contracts::Usage>(raw_value));
          break;
        case kTagUsageMin:
          local_state.usage_min = static_cast<charm::contracts::Usage>(raw_value);
          local_state.has_usage_range = true;
          break;
        case kTagUsageMax:
          local_state.usage_max = static_cast<charm::contracts::Usage>(raw_value);
          local_state.has_usage_range = true;
          break;
        default:
          break;
      }
    } else if (type == ItemType::kMain) {
      switch (tag) {
        case kTagCollection: {
          if (model.collection_count < kMaxCollectionsPerInterface) {
            CollectionDescriptor& col = model.collections[model.collection_count];
            col.usage_page = global_state.usage_page;
            col.usage = local_state.usage_count > 0 ? local_state.usages[0] : 0;
            col.collection_index = next_collection_index++;

            if (raw_value == 0x01) col.kind = CollectionKind::kApplication;
            else if (raw_value == 0x02) col.kind = CollectionKind::kLogical;
            else if (raw_value == 0x00) col.kind = CollectionKind::kPhysical;
            else col.kind = CollectionKind::kUnknown;

            if (collection_depth < collection_stack.size()) {
              collection_stack[collection_depth] = col.collection_index;
            }
            collection_depth++;
            model.collection_count++;
          } else {
            result.status = charm::contracts::ContractStatus::kRejected;
            result.fault_code = charm::contracts::FaultCode{
                .category = charm::contracts::ErrorCategory::kCapacityExceeded,
                .reason = 3}; // Collections capacity exceeded
            return result;
          }
          local_state.Clear();
          break;
        }
        case kTagEndCollection: {
          if (collection_depth > 0) {
            collection_depth--;
          }
          local_state.Clear();
          break;
        }
        case kTagInput:
        case kTagOutput:
        case kTagFeature: {
          bool is_constant = (raw_value & 0x01) != 0;
          std::uint16_t* target_bit_offset = &bit_offset_input;
          if (tag == kTagOutput) {
            target_bit_offset = &bit_offset_output;
          } else if (tag == kTagFeature) {
            target_bit_offset = &bit_offset_feature;
          }

          if (!is_constant) {
            for (std::uint16_t i = 0; i < global_state.report_count; ++i) {
              if (model.field_count < kMaxFieldsPerInterface) {
                FieldDescriptor& field = model.fields[model.field_count++];
                field.report_id = global_state.report_id;
                field.usage_page = global_state.usage_page;

                if (local_state.has_usage_range) {
                  field.usage = local_state.usage_min + i;
                  if (field.usage > local_state.usage_max) {
                    field.usage = local_state.usage_max;
                  }
                } else if (i < local_state.usage_count) {
                  field.usage = local_state.usages[i];
                } else if (local_state.usage_count > 0) {
                  field.usage = local_state.usages[local_state.usage_count - 1];
                } else {
                  field.usage = 0;
                }

                field.collection_index = collection_depth > 0 ? collection_stack[collection_depth - 1] : 0;
                field.logical_index = i;
                field.bit_offset = *target_bit_offset;
                field.bit_size = global_state.report_size;
                field.is_signed = (global_state.logical_min < 0);
              } else {
                result.status = charm::contracts::ContractStatus::kRejected;
                result.fault_code = charm::contracts::FaultCode{
                    .category = charm::contracts::ErrorCategory::kCapacityExceeded,
                    .reason = 4}; // Fields capacity exceeded
                return result;
              }
              *target_bit_offset += global_state.report_size;
            }
          } else {
            *target_bit_offset += (global_state.report_size * global_state.report_count);
          }
          local_state.Clear();
          break;
        }
      }
    }
  }

  result.status = charm::contracts::ContractStatus::kOk;
  result.semantic_model = model;
  return result;
}

}  // namespace charm::core
