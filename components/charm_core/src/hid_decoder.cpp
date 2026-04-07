#include "charm/core/hid_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace charm::core {

namespace {

// Helper to extract bit fields across byte boundaries
std::uint32_t ExtractBits(const std::uint8_t* buffer, std::size_t buffer_len, std::uint16_t bit_offset, std::uint16_t bit_size) {
  if (bit_size == 0) return 0;

  std::uint32_t result = 0;
  std::size_t bits_read = 0;

  while (bits_read < bit_size) {
    std::uint16_t current_bit_index = bit_offset + bits_read;
    std::size_t byte_index = current_bit_index / 8;

    // Safety check against buffer overrun.
    if (byte_index >= buffer_len) {
      break;
    }

    std::uint8_t bit_in_byte = current_bit_index % 8;
    std::size_t bits_to_read = std::min(static_cast<std::size_t>(8 - bit_in_byte), static_cast<std::size_t>(bit_size - bits_read));

    std::uint8_t mask = (1 << bits_to_read) - 1;
    std::uint8_t byte_val = (buffer[byte_index] >> bit_in_byte) & mask;

    result |= (static_cast<std::uint32_t>(byte_val) << bits_read);
    bits_read += bits_to_read;
  }

  return result;
}

std::int32_t SignExtend(std::uint32_t value, std::uint16_t bit_size) {
  if (bit_size == 0 || bit_size >= 32) return static_cast<std::int32_t>(value);

  std::uint32_t sign_bit = 1U << (bit_size - 1);
  if ((value & sign_bit) != 0) {
    // Extend the sign
    std::uint32_t mask = ~((1U << bit_size) - 1);
    value |= mask;
  }

  return static_cast<std::int32_t>(value);
}

std::uint64_t MakeArrayStateKey(const charm::contracts::RawHidReportRef& report,
                                const DecodeBinding& binding) {
  std::uint32_t mixed = 0x811C9DC5u;
  auto mix = [&mixed](std::uint32_t value) {
    mixed ^= value;
    mixed *= 0x01000193u;
  };
  mix(report.device_handle.value);
  mix(binding.report_id);
  mix(binding.element_key.usage_page);
  mix(binding.usage_min);
  mix(binding.usage_max);
  mix(binding.element_key.collection_index);
  return (static_cast<std::uint64_t>(report.interface_handle.value) << 32u) |
         mixed;
}

std::int32_t ClampToLogicalRange(std::int32_t value, const DecodeBinding& binding) {
  if (binding.logical_min >= binding.logical_max) {
    return value;
  }
  return std::clamp(value, binding.logical_min, binding.logical_max);
}

std::int32_t NormalizeAxis(std::int32_t raw_value, const DecodeBinding& binding) {
  raw_value = ClampToLogicalRange(raw_value, binding);
  if (binding.logical_min >= binding.logical_max) {
    return raw_value;
  }
  const auto input_span =
      static_cast<std::int64_t>(binding.logical_max) - binding.logical_min;
  const auto shifted = static_cast<std::int64_t>(raw_value) - binding.logical_min;
  const auto scaled = (shifted * 254ll) / input_span - 127ll;
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(scaled, -127ll, 127ll));
}

std::int32_t NormalizeTrigger(std::int32_t raw_value, const DecodeBinding& binding) {
  raw_value = ClampToLogicalRange(raw_value, binding);
  if (binding.logical_min >= binding.logical_max) {
    return std::clamp<std::int32_t>(raw_value, 0, 255);
  }
  const auto input_span =
      static_cast<std::int64_t>(binding.logical_max) - binding.logical_min;
  const auto shifted =
      std::max<std::int64_t>(0, static_cast<std::int64_t>(raw_value) - binding.logical_min);
  const auto scaled = (shifted * 255ll) / input_span;
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(scaled, 0ll, 255ll));
}

std::int32_t NormalizeHat(std::int32_t raw_value, const DecodeBinding& binding) {
  if (binding.logical_min > binding.logical_max) {
    return raw_value;
  }
  if (binding.has_null_state &&
      (raw_value < binding.logical_min || raw_value > binding.logical_max)) {
    return binding.logical_max + 1;
  }
  return std::clamp(raw_value, binding.logical_min, binding.logical_max);
}

std::int32_t NormalizeValue(std::int32_t raw_value, const DecodeBinding& binding) {
  switch (binding.element_type) {
    case charm::contracts::InputElementType::kButton:
      return raw_value != 0 ? 1 : 0;
    case charm::contracts::InputElementType::kTrigger:
      return NormalizeTrigger(raw_value, binding);
    case charm::contracts::InputElementType::kHat:
      return NormalizeHat(raw_value, binding);
    case charm::contracts::InputElementType::kAxis:
      return NormalizeAxis(raw_value, binding);
    case charm::contracts::InputElementType::kScalar:
      return ClampToLogicalRange(raw_value, binding);
    default:
      return raw_value;
  }
}

void PushEvent(const charm::contracts::InputElementEvent& event,
               charm::contracts::InputElementEvent* buffer,
               std::size_t capacity,
               std::size_t* count,
               DecodeReportResult* result) {
  if (*count >= capacity) {
    result->status = charm::contracts::ContractStatus::kRejected;
    result->fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kCapacityExceeded,
        .reason = 9};
    return;
  }

  if (buffer != nullptr) {
    buffer[*count] = event;
  }
  ++(*count);
}

}  // namespace

DecodeReportResult DefaultHidDecoder::DecodeReport(const DecodeReportRequest& request) {
  DecodeReportResult result;

  if (request.decode_plan == nullptr) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kContractViolation,
        .reason = 1}; // Decode plan missing
    return result;
  }

  if (request.report.bytes == nullptr || request.report.byte_length == 0) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kInvalidRequest,
        .reason = 2}; // Report malformed/empty
    return result;
  }

  if (request.report.byte_length != request.report.report_meta.declared_length) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kInvalidRequest,
        .reason = 3}; // Report length mismatch
    return result;
  }

  if (request.decode_plan->binding_count > kMaxDecodeBindingsPerInterface) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code = charm::contracts::FaultCode{
        .category = charm::contracts::ErrorCategory::kCapacityExceeded,
        .reason = 7}; // Too many bindings
    return result;
  }

  std::size_t current_event_count = 0;
  std::unordered_map<std::uint64_t, std::unordered_set<charm::contracts::Usage>>
      current_array_usages{};
  std::unordered_map<std::uint64_t, DecodeBinding> array_bindings{};

  for (std::size_t i = 0; i < request.decode_plan->binding_count; ++i) {
    const auto& binding = request.decode_plan->bindings[i];

    if (binding.report_id != request.report.report_meta.report_id) {
      continue;
    }

    if (binding.bit_size > 32) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kInvalidRequest,
          .reason = 6}; // Binding size too large
      return result;
    }

    // Check if the bit extraction goes out of bounds
    std::size_t max_byte_required = (binding.bit_offset + binding.bit_size - 1) / 8;
    if (max_byte_required >= request.report.byte_length) {
      result.status = charm::contracts::ContractStatus::kRejected;
      result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kInvalidRequest,
          .reason = 5}; // Binding requires reading beyond report bounds
      return result;
    }

    std::uint32_t raw_value = ExtractBits(request.report.bytes, request.report.byte_length, binding.bit_offset, binding.bit_size);
    std::int32_t final_value =
        binding.is_signed ? SignExtend(raw_value, binding.bit_size)
                          : static_cast<std::int32_t>(raw_value);

    if (binding.is_array) {
      const auto state_key = MakeArrayStateKey(request.report, binding);
      current_array_usages.try_emplace(state_key);
      array_bindings.emplace(state_key, binding);
      if (final_value == 0) {
        continue;
      }
      if (binding.has_usage_range &&
          (static_cast<charm::contracts::Usage>(final_value) < binding.usage_min ||
           static_cast<charm::contracts::Usage>(final_value) > binding.usage_max)) {
        continue;
      }
      current_array_usages[state_key].insert(
          static_cast<charm::contracts::Usage>(final_value));
      continue;
    }

    charm::contracts::InputElementEvent event{};
    event.element_key_hash = binding.element_key_hash;
    event.element_type = binding.element_type;
    event.value = NormalizeValue(final_value, binding);
    event.timestamp = request.report.timestamp;
    event.device_handle = request.report.device_handle;
    event.interface_handle = request.report.interface_handle;

    PushEvent(event, request.events_buffer, request.events_buffer_capacity,
              &current_event_count, &result);
    if (result.status == charm::contracts::ContractStatus::kRejected) {
      return result;
    }
  }

  std::vector<std::uint64_t> ordered_group_keys;
  ordered_group_keys.reserve(current_array_usages.size());
  for (const auto& [group_key, _] : current_array_usages) {
    ordered_group_keys.push_back(group_key);
  }
  std::sort(ordered_group_keys.begin(), ordered_group_keys.end());

  for (const auto& group_key : ordered_group_keys) {
    const auto binding_it = array_bindings.find(group_key);
    if (binding_it == array_bindings.end()) {
      continue;
    }
    const auto& binding = binding_it->second;
    const auto previous_it = array_active_usages_.find(group_key);
    const std::unordered_set<charm::contracts::Usage> empty_previous{};
    const auto& previous =
        previous_it != array_active_usages_.end() ? previous_it->second : empty_previous;
    const auto& current = current_array_usages[group_key];

    std::vector<charm::contracts::Usage> releases;
    releases.reserve(previous.size());
    for (const auto usage : previous) {
      if (current.find(usage) == current.end()) {
        releases.push_back(usage);
      }
    }
    std::sort(releases.begin(), releases.end());

    for (const auto usage : releases) {
      charm::contracts::InputElementEvent event{};
      event.element_key_hash =
          ComputeElementKeyHash(MakeElementKeyForUsage(binding, usage));
      event.element_type = binding.element_type;
      event.value = 0;
      event.timestamp = request.report.timestamp;
      event.device_handle = request.report.device_handle;
      event.interface_handle = request.report.interface_handle;
      PushEvent(event, request.events_buffer, request.events_buffer_capacity,
                &current_event_count, &result);
      if (result.status == charm::contracts::ContractStatus::kRejected) {
        return result;
      }
    }

    std::vector<charm::contracts::Usage> presses;
    presses.reserve(current.size());
    for (const auto usage : current) {
      if (previous.find(usage) == previous.end()) {
        presses.push_back(usage);
      }
    }
    std::sort(presses.begin(), presses.end());

    for (const auto usage : presses) {
      charm::contracts::InputElementEvent event{};
      event.element_key_hash =
          ComputeElementKeyHash(MakeElementKeyForUsage(binding, usage));
      event.element_type = binding.element_type;
      event.value = 1;
      event.timestamp = request.report.timestamp;
      event.device_handle = request.report.device_handle;
      event.interface_handle = request.report.interface_handle;
      PushEvent(event, request.events_buffer, request.events_buffer_capacity,
                &current_event_count, &result);
      if (result.status == charm::contracts::ContractStatus::kRejected) {
        return result;
      }
    }

    if (current.empty()) {
      array_active_usages_.erase(group_key);
    } else {
      array_active_usages_[group_key] = current;
    }
  }

  result.status = charm::contracts::ContractStatus::kOk;
  result.events = request.events_buffer;
  result.event_count = current_event_count;

  return result;
}

void DefaultHidDecoder::ResetInterfaceState(
    charm::contracts::InterfaceHandle interface_handle) {
  for (auto it = array_active_usages_.begin(); it != array_active_usages_.end();) {
    const std::uint32_t encoded_interface =
        static_cast<std::uint32_t>((it->first >> 32) & 0xffffffffull);
    if (encoded_interface == interface_handle.value) {
      it = array_active_usages_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace charm::core
