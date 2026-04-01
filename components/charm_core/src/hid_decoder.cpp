#include "charm/core/hid_decoder.hpp"

#include <cstdint>
#include <algorithm>
#include <cstring>

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

} // namespace

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

  for (std::size_t i = 0; i < request.decode_plan->binding_count; ++i) {
    const auto& binding = request.decode_plan->bindings[i];

    if (binding.report_id != request.report.report_meta.report_id) {
      continue;
    }

    if (current_event_count >= kMaxDecodeBindingsPerInterface) {
       result.status = charm::contracts::ContractStatus::kRejected;
       result.fault_code = charm::contracts::FaultCode{
          .category = charm::contracts::ErrorCategory::kCapacityExceeded,
          .reason = 4}; // Event count capacity exceeded
       return result;
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

    std::int32_t final_value;
    if (binding.is_signed) {
      final_value = SignExtend(raw_value, binding.bit_size);
    } else {
      final_value = static_cast<std::int32_t>(raw_value);
    }

    if (request.events_buffer != nullptr && current_event_count < request.events_buffer_capacity) {
      auto& event = request.events_buffer[current_event_count];
      event.element_key_hash = binding.element_key_hash;
      event.element_type = binding.element_type;
      event.value = final_value;
      event.timestamp = request.report.timestamp;
      event.device_handle = request.report.device_handle;
      event.interface_handle = request.report.interface_handle;
    }

    current_event_count++;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  result.events = request.events_buffer;
  result.event_count = std::min(current_event_count, request.events_buffer_capacity);

  return result;
}

}  // namespace charm::core
