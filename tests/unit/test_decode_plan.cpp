#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

#include "charm/core/decode_plan.hpp"

using namespace charm::core;
using namespace charm::contracts;

void test_build_simple_plan() {
    DefaultDecodePlanBuilder builder;
    BuildDecodePlanRequest req;

    // Simulate a basic semantic model with 1 axis and 1 button
    req.input.device_handle.value = 1;
    req.input.interface_handle.value = 1;
    req.input.interface_number = 0;

    req.input.semantic_model.field_count = 2;

    // Axis X
    req.input.semantic_model.fields[0].report_id = 1;
    req.input.semantic_model.fields[0].usage_page = 0x01; // Generic Desktop
    req.input.semantic_model.fields[0].usage = 0x30; // X
    req.input.semantic_model.fields[0].collection_index = 1;
    req.input.semantic_model.fields[0].logical_index = 0;
    req.input.semantic_model.fields[0].bit_offset = 0;
    req.input.semantic_model.fields[0].bit_size = 8;
    req.input.semantic_model.fields[0].is_signed = false;

    // Button 1
    req.input.semantic_model.fields[1].report_id = 1;
    req.input.semantic_model.fields[1].usage_page = 0x09; // Button
    req.input.semantic_model.fields[1].usage = 1; // Button 1
    req.input.semantic_model.fields[1].collection_index = 1;
    req.input.semantic_model.fields[1].logical_index = 1;
    req.input.semantic_model.fields[1].bit_offset = 8;
    req.input.semantic_model.fields[1].bit_size = 1;
    req.input.semantic_model.fields[1].is_signed = false;

    auto result = builder.BuildDecodePlan(req);
    assert(result.status == ContractStatus::kOk);
    assert(result.decode_plan.binding_count == 2);

    // Verify Axis
    const auto& axis_binding = result.decode_plan.bindings[0];
    assert(axis_binding.element_type == InputElementType::kAxis);
    assert(axis_binding.report_id == 1);
    assert(axis_binding.bit_offset == 0);
    assert(axis_binding.bit_size == 8);
    assert(axis_binding.element_key.usage_page == 0x01);
    assert(axis_binding.element_key.usage == 0x30);
    assert(axis_binding.element_key_hash.value != 0);

    // Verify Button
    const auto& btn_binding = result.decode_plan.bindings[1];
    assert(btn_binding.element_type == InputElementType::kButton);
    assert(btn_binding.report_id == 1);
    assert(btn_binding.bit_offset == 8);
    assert(btn_binding.bit_size == 1);
    assert(btn_binding.element_key.usage_page == 0x09);
    assert(btn_binding.element_key.usage == 1);
    assert(btn_binding.element_key_hash.value != 0);

    // Verify Hashes differ
    assert(axis_binding.element_key_hash.value != btn_binding.element_key_hash.value);
}

void test_capacity_exceeded() {
    DefaultDecodePlanBuilder builder;
    BuildDecodePlanRequest req;

    // Simulate exceeding bindings capacity
    req.input.semantic_model.field_count = kMaxDecodeBindingsPerInterface + 1;

    auto result = builder.BuildDecodePlan(req);
    assert(result.status == ContractStatus::kRejected);
    assert(result.fault_code.category == ErrorCategory::kCapacityExceeded);
}

void test_element_type_determination() {
    DefaultDecodePlanBuilder builder;
    BuildDecodePlanRequest req;

    req.input.semantic_model.field_count = 3;

    // Hat switch
    req.input.semantic_model.fields[0].usage_page = 0x01;
    req.input.semantic_model.fields[0].usage = 0x39;

    // Unmapped scalar
    req.input.semantic_model.fields[1].usage_page = 0x01;
    req.input.semantic_model.fields[1].usage = 0x40; // Beyond normal axes

    // Trigger / scalar? For now, our simple heuristic maps it to Scalar unless it's Axis/Hat/Button.
    req.input.semantic_model.fields[2].usage_page = 0x02; // Simulation controls
    req.input.semantic_model.fields[2].usage = 0xC4; // Accelerator

    auto result = builder.BuildDecodePlan(req);
    assert(result.status == ContractStatus::kOk);
    assert(result.decode_plan.bindings[0].element_type == InputElementType::kHat);
    assert(result.decode_plan.bindings[1].element_type == InputElementType::kScalar);
    assert(result.decode_plan.bindings[2].element_type == InputElementType::kScalar);
}

int main() {
    test_build_simple_plan();
    test_capacity_exceeded();
    test_element_type_determination();
    std::cout << "All decode_plan tests passed!" << std::endl;
    return 0;
}
