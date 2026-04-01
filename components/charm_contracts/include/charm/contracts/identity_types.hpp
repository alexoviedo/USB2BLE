#pragma once

#include <array>
#include <cstdint>

#include "charm/contracts/common_types.hpp"

namespace charm::contracts {

struct __attribute__((packed)) HubPath {
  std::array<PortNumber, kMaxHubPathDepth> ports{};
  std::uint8_t depth{0};
};

struct __attribute__((packed)) ElementKey {
  VendorId vendor_id{0};
  ProductId product_id{0};
  HubPath hub_path{};
  InterfaceNumber interface_number{0};
  ReportId report_id{0};
  UsagePage usage_page{0};
  Usage usage{0};
  CollectionIndex collection_index{0};
  LogicalIndex logical_index{0};
};

struct ElementKeyHash {
  std::uint64_t value{0};
};

struct DeviceHandle {
  std::uint32_t value{0};
};

struct InterfaceHandle {
  std::uint32_t value{0};
};

struct ProfileId {
  std::uint32_t value{0};
};

struct MappingBundleRef {
  BundleId bundle_id{0};
  BundleVersion version{0};
  IntegrityValue integrity{0};
};

}  // namespace charm::contracts
