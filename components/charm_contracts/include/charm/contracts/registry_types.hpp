#pragma once
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/common_types.hpp"
#include "charm/contracts/status_types.hpp"
namespace charm::contracts {
struct ActiveProfileRef { ProfileId profile_id{}; };
struct ActiveMappingBundleRef { MappingBundleRef mapping_bundle{}; };
struct FaultRecordRef { FaultCode fault_code{}; FaultSeverity severity{FaultSeverity::kInfo}; Timestamp timestamp{}; };
struct DecodePlanRef { const void* plan{nullptr}; };
struct RegistryEntry { DeviceHandle device_handle{}; InterfaceHandle interface_handle{}; InterfaceNumber interface_number{0}; DecodePlanRef decode_plan{}; };
}  // namespace charm::contracts
