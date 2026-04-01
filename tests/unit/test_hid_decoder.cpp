#include <gtest/gtest.h>

#include "charm/contracts/events.hpp"
#include "charm/contracts/report_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/core/decode_plan.hpp"
#include "charm/core/hid_decoder.hpp"

using namespace charm::contracts;
using namespace charm::core;

class HidDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    decoder_ = std::make_unique<DefaultHidDecoder>();
    request_.events_buffer = events_buffer_;
    request_.events_buffer_capacity = kMaxDecodeBindingsPerInterface;
  }

  std::unique_ptr<DefaultHidDecoder> decoder_;
  DecodeReportRequest request_{};
  charm::contracts::InputElementEvent events_buffer_[kMaxDecodeBindingsPerInterface]{};
};

TEST_F(HidDecoderTest, MissingDecodePlanReturnsRejected) {
  DecodeReportRequest request = request_;
  request.decode_plan = nullptr;
  request.report.bytes = reinterpret_cast<const std::uint8_t*>("test");
  request.report.byte_length = 4;
  request.report.report_meta.declared_length = 4;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kContractViolation);
  EXPECT_EQ(result.fault_code.reason, 1);
  EXPECT_EQ(result.event_count, 0);
}

TEST_F(HidDecoderTest, EmptyReportReturnsRejected) {
  DecodePlan plan;
  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = nullptr;
  request.report.byte_length = 0;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);
  EXPECT_EQ(result.fault_code.reason, 2);
  EXPECT_EQ(result.event_count, 0);
}

TEST_F(HidDecoderTest, ReportLengthMismatchReturnsRejected) {
  DecodePlan plan;
  std::uint8_t buffer[] = {0x00, 0x01};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 2;
  request.report.report_meta.declared_length = 4; // Mismatch

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);
  EXPECT_EQ(result.fault_code.reason, 3);
  EXPECT_EQ(result.event_count, 0);
}

TEST_F(HidDecoderTest, DecodesUnsignedButtonsCorrectly) {
  DecodePlan plan;
  plan.binding_count = 2;

  // Binding 0: 1-bit at offset 0
  plan.bindings[0].report_id = 1;
  plan.bindings[0].bit_offset = 0;
  plan.bindings[0].bit_size = 1;
  plan.bindings[0].is_signed = false;
  plan.bindings[0].element_key_hash.value = 100;
  plan.bindings[0].element_type = InputElementType::kButton;

  // Binding 1: 1-bit at offset 2
  plan.bindings[1].report_id = 1;
  plan.bindings[1].bit_offset = 2;
  plan.bindings[1].bit_size = 1;
  plan.bindings[1].is_signed = false;
  plan.bindings[1].element_key_hash.value = 101;
  plan.bindings[1].element_type = InputElementType::kButton;

  // Report byte: 0b00000101 -> bit 0 is 1, bit 2 is 1, bit 1 is 0
  std::uint8_t buffer[] = {0x05};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 1;
  request.report.report_meta.report_id = 1;
  request.report.report_meta.declared_length = 1;
  request.report.timestamp.ticks = 12345;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.event_count, 2);

  EXPECT_EQ(result.events[0].element_key_hash.value, 100);
  EXPECT_EQ(result.events[0].value, 1);
  EXPECT_EQ(result.events[0].timestamp.ticks, 12345);

  EXPECT_EQ(result.events[1].element_key_hash.value, 101);
  EXPECT_EQ(result.events[1].value, 1);
  EXPECT_EQ(result.events[1].timestamp.ticks, 12345);
}

TEST_F(HidDecoderTest, DecodesSignedAxisCorrectly) {
  DecodePlan plan;
  plan.binding_count = 1;

  // Binding 0: 8-bit signed at offset 8 (byte 1)
  plan.bindings[0].report_id = 2;
  plan.bindings[0].bit_offset = 8;
  plan.bindings[0].bit_size = 8;
  plan.bindings[0].is_signed = true;
  plan.bindings[0].element_key_hash.value = 200;
  plan.bindings[0].element_type = InputElementType::kAxis;

  // Byte 0: ignored, Byte 1: 0xFE (-2 in 8-bit signed)
  std::uint8_t buffer[] = {0x00, 0xFE};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 2;
  request.report.report_meta.report_id = 2;
  request.report.report_meta.declared_length = 2;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.event_count, 1);
  EXPECT_EQ(result.events[0].element_key_hash.value, 200);
  EXPECT_EQ(result.events[0].value, -2);
}

TEST_F(HidDecoderTest, SkipsBindingsForOtherReportIds) {
  DecodePlan plan;
  plan.binding_count = 2;

  plan.bindings[0].report_id = 1; // Different ID
  plan.bindings[0].bit_offset = 0;
  plan.bindings[0].bit_size = 8;
  plan.bindings[0].is_signed = false;

  plan.bindings[1].report_id = 2; // Matching ID
  plan.bindings[1].bit_offset = 0;
  plan.bindings[1].bit_size = 8;
  plan.bindings[1].is_signed = false;
  plan.bindings[1].element_key_hash.value = 300;

  std::uint8_t buffer[] = {0x42};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 1;
  request.report.report_meta.report_id = 2; // Notice the report is ID 2
  request.report.report_meta.declared_length = 1;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.event_count, 1); // Only binding 1 should be processed
  EXPECT_EQ(result.events[0].element_key_hash.value, 300);
  EXPECT_EQ(result.events[0].value, 0x42);
}

TEST_F(HidDecoderTest, ReadingBeyondBoundsReturnsRejected) {
  DecodePlan plan;
  plan.binding_count = 1;

  // Binding asks for bits up to bit 16, requiring 3 bytes of data, but we only supply 2.
  plan.bindings[0].report_id = 1;
  plan.bindings[0].bit_offset = 8;
  plan.bindings[0].bit_size = 9; // up to bit 16
  plan.bindings[0].is_signed = false;

  std::uint8_t buffer[] = {0x00, 0xFF};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 2;
  request.report.report_meta.report_id = 1;
  request.report.report_meta.declared_length = 2;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kRejected);
  EXPECT_EQ(result.fault_code.category, ErrorCategory::kInvalidRequest);
  EXPECT_EQ(result.fault_code.reason, 5); // Read out of bounds
  EXPECT_EQ(result.event_count, 0);
}

TEST_F(HidDecoderTest, DecodesCrossByteBitsCorrectly) {
  DecodePlan plan;
  plan.binding_count = 1;

  // Starts at bit 4, size 12 bits -> bits 4 through 15. Requires 2 bytes.
  plan.bindings[0].report_id = 1;
  plan.bindings[0].bit_offset = 4;
  plan.bindings[0].bit_size = 12;
  plan.bindings[0].is_signed = false;
  plan.bindings[0].element_key_hash.value = 400;

  // Byte 0: 0x50 (0b01010000) -> top 4 bits are 0101
  // Byte 1: 0xA3 (0b10100011) -> 8 bits
  // Expected 12 bit val: 0xA35 = 2613
  std::uint8_t buffer[] = {0x50, 0xA3};

  DecodeReportRequest request = request_;
  request.decode_plan = &plan;
  request.report.bytes = buffer;
  request.report.byte_length = 2;
  request.report.report_meta.report_id = 1;
  request.report.report_meta.declared_length = 2;

  auto result = decoder_->DecodeReport(request);

  EXPECT_EQ(result.status, ContractStatus::kOk);
  ASSERT_EQ(result.event_count, 1);
  EXPECT_EQ(result.events[0].value, 0xA35);
}
