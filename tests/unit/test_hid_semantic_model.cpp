#include <cassert>
#include <iostream>
#include <vector>

#include "charm/core/hid_semantic_model.hpp"

using namespace charm::core;
using namespace charm::contracts;

void test_empty_descriptor() {
    DefaultHidDescriptorParser parser;
    ParseDescriptorRequest req;
    req.descriptor.bytes = nullptr;
    req.descriptor.size = 0;

    auto result = parser.ParseDescriptor(req);
    assert(result.status == ContractStatus::kRejected);
    assert(result.fault_code.category == ErrorCategory::kInvalidRequest);
}

void test_simple_gamepad() {
    // Simple descriptor: 2 buttons, 2 axes
    // Usage Page (Generic Desktop)
    // Usage (Gamepad)
    // Collection (Application)
    //   Usage Page (Button)
    //   Usage Min (1), Usage Max (2)
    //   Logical Min (0), Logical Max (1)
    //   Report Size (1), Report Count (2)
    //   Input (Data, Var, Abs)
    //   Report Size (6), Report Count (1)
    //   Input (Cnst, Var, Abs)
    //   Usage Page (Generic Desktop)
    //   Usage (X), Usage (Y)
    //   Logical Min (0), Logical Max (255)
    //   Report Size (8), Report Count (2)
    //   Input (Data, Var, Abs)
    // End Collection
    std::vector<std::uint8_t> desc = {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x02,        //   Usage Maximum (0x02)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x75, 0x06,        //   Report Size (6)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xC0,              // End Collection
    };

    DefaultHidDescriptorParser parser;
    ParseDescriptorRequest req;
    req.descriptor.bytes = desc.data();
    req.descriptor.size = desc.size();

    auto result = parser.ParseDescriptor(req);
    assert(result.status == ContractStatus::kOk);

    // We should have 1 collection
    assert(result.semantic_model.collection_count == 1);
    assert(result.semantic_model.collections[0].usage_page == 1);
    assert(result.semantic_model.collections[0].usage == 5);
    assert(result.semantic_model.collections[0].kind == CollectionKind::kApplication);

    // We should have 4 fields (2 buttons, 2 axes). The 6-bit const field is skipped.
    assert(result.semantic_model.field_count == 4);

    // Button 1
    assert(result.semantic_model.fields[0].usage_page == 9);
    assert(result.semantic_model.fields[0].usage == 1);
    assert(result.semantic_model.fields[0].bit_offset == 0);
    assert(result.semantic_model.fields[0].bit_size == 1);

    // Button 2
    assert(result.semantic_model.fields[1].usage_page == 9);
    assert(result.semantic_model.fields[1].usage == 2);
    assert(result.semantic_model.fields[1].bit_offset == 1);
    assert(result.semantic_model.fields[1].bit_size == 1);

    // Axis X
    assert(result.semantic_model.fields[2].usage_page == 1);
    assert(result.semantic_model.fields[2].usage == 0x30);
    assert(result.semantic_model.fields[2].bit_offset == 8); // 2 buttons + 6 const bits = 8 bits
    assert(result.semantic_model.fields[2].bit_size == 8);

    // Axis Y
    assert(result.semantic_model.fields[3].usage_page == 1);
    assert(result.semantic_model.fields[3].usage == 0x31);
    assert(result.semantic_model.fields[3].bit_offset == 16);
    assert(result.semantic_model.fields[3].bit_size == 8);
}

void test_malformed_descriptor() {
    std::vector<std::uint8_t> desc = {
        0x05, 0x01,        // Usage Page
        0x09,              // Usage (malformed, missing payload byte)
    };

    DefaultHidDescriptorParser parser;
    ParseDescriptorRequest req;
    req.descriptor.bytes = desc.data();
    req.descriptor.size = desc.size();

    auto result = parser.ParseDescriptor(req);
    assert(result.status == ContractStatus::kRejected);
    assert(result.fault_code.category == ErrorCategory::kContractViolation);
}

int main() {
    test_empty_descriptor();
    test_simple_gamepad();
    test_malformed_descriptor();
    std::cout << "All hid_semantic_model tests passed!" << std::endl;
    return 0;
}
