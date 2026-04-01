#pragma once

#include <cstddef>
#include <cstdint>

namespace charm::contracts {

using VendorId = std::uint16_t;
using ProductId = std::uint16_t;
using PortNumber = std::uint8_t;
using InterfaceNumber = std::uint8_t;
using ReportId = std::uint8_t;
using UsagePage = std::uint16_t;
using Usage = std::uint16_t;
using CollectionIndex = std::uint16_t;
using LogicalIndex = std::uint16_t;
using BundleId = std::uint32_t;
using BundleVersion = std::uint32_t;
using IntegrityValue = std::uint32_t;

inline constexpr std::size_t kMaxHubPathDepth = 8;

struct Timestamp {
  std::uint64_t ticks{0};
};

struct Duration {
  std::uint64_t ticks{0};
};

}  // namespace charm::contracts
