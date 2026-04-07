#include "charm/app/runtime_data_plane.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_set>

#include "charm/core/config_compiler.hpp"

#if __has_include("esp_log.h")
#include "esp_log.h"
#define CHARM_RUNTIME_DATA_PLANE_LOGI(fmt, ...) ESP_LOGI("runtime_data_plane", fmt, ##__VA_ARGS__)
#define CHARM_RUNTIME_DATA_PLANE_LOGW(fmt, ...) ESP_LOGW("runtime_data_plane", fmt, ##__VA_ARGS__)
#define CHARM_RUNTIME_DATA_PLANE_LOGD(fmt, ...) ESP_LOGD("runtime_data_plane", fmt, ##__VA_ARGS__)
#else
#define CHARM_RUNTIME_DATA_PLANE_LOGI(fmt, ...) std::fprintf(stderr, "[runtime_data_plane][I] " fmt "\n", ##__VA_ARGS__)
#define CHARM_RUNTIME_DATA_PLANE_LOGW(fmt, ...) std::fprintf(stderr, "[runtime_data_plane][W] " fmt "\n", ##__VA_ARGS__)
#define CHARM_RUNTIME_DATA_PLANE_LOGD(fmt, ...) std::fprintf(stderr, "[runtime_data_plane][D] " fmt "\n", ##__VA_ARGS__)
#endif

namespace charm::app {

namespace {

constexpr charm::contracts::ProfileId kDefaultProfileId{1};
constexpr std::size_t kMaxRuntimeInterfaces = 32;
constexpr std::size_t kMaxRuntimeSources = 512;
constexpr std::uint32_t kDefaultBundleSeed = 1;
constexpr std::uint16_t kMaxHatMappings = 1;
// The currently shipped Generic Gamepad profile exposes 16 button bits.
constexpr std::size_t kMaxEncodableButtons = 16;

std::pair<std::int32_t, std::int32_t> DefaultClampRange(
    charm::core::LogicalElementType target_type) {
  switch (target_type) {
    case charm::core::LogicalElementType::kAxis:
      return {-127, 127};
    case charm::core::LogicalElementType::kButton:
      return {0, 1};
    case charm::core::LogicalElementType::kTrigger:
      return {0, 255};
    case charm::core::LogicalElementType::kHat:
      return {0, 8};
    default:
      return {-127, 127};
  }
}

std::int32_t NormalizeForCompiledBundle(
    const charm::contracts::InputElementEvent& event,
    charm::contracts::InputElementType canonical_source_type) {
  if (canonical_source_type == charm::contracts::InputElementType::kButton) {
    return event.value != 0 ? 1 : 0;
  }
  if (canonical_source_type != charm::contracts::InputElementType::kAxis) {
    return event.value;
  }

  if (event.element_type == charm::contracts::InputElementType::kTrigger) {
    const auto scaled = (static_cast<std::int64_t>(event.value) * 127ll) / 255ll;
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(scaled, 0ll, 127ll));
  }

  return std::clamp<std::int32_t>(event.value, -127, 127);
}

}  // namespace

RuntimeDataPlane::RuntimeDataPlane(charm::ports::UsbHostPort& usb_host,
                                   charm::ports::BleTransportPort& ble_transport,
                                   charm::core::DeviceRegistry& device_registry,
                                   charm::core::HidDescriptorParser& descriptor_parser,
                                   charm::core::DecodePlanBuilder& decode_plan_builder,
                                   charm::core::HidDecoder& hid_decoder,
                                   charm::core::MappingEngine& mapping_engine,
                                   charm::core::MappingBundleLoader& mapping_bundle_loader,
                                   charm::core::ProfileManager& profile_manager,
                                   charm::core::Supervisor& supervisor)
    : usb_host_(usb_host),
      ble_transport_(ble_transport),
      device_registry_(device_registry),
      descriptor_parser_(descriptor_parser),
      decode_plan_builder_(decode_plan_builder),
      hid_decoder_(hid_decoder),
      mapping_engine_(mapping_engine),
      mapping_bundle_loader_(mapping_bundle_loader),
      profile_manager_(profile_manager),
      supervisor_(supervisor) {}

void RuntimeDataPlane::OnDeviceConnected(
    const charm::ports::UsbEnumerationInfo& enumeration_info,
    const charm::ports::DeviceDescriptorRef& device_descriptor) {
  CHARM_RUNTIME_DATA_PLANE_LOGI(
      "device connected handle=%u vid=0x%04x pid=0x%04x desc_bytes=%u",
      enumeration_info.device_handle.value, enumeration_info.vendor_id,
      enumeration_info.product_id,
      static_cast<unsigned>(device_descriptor.descriptor.size));
  charm::core::RegisterDeviceRequest register_request{};
  register_request.enumeration_info = enumeration_info;
  register_request.device_descriptor = device_descriptor;
  const auto register_result = device_registry_.RegisterDevice(register_request);
  if (register_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("device registration rejected handle=%u reason=%u",
                                  enumeration_info.device_handle.value,
                                  register_result.fault_code.reason);
  }
}

void RuntimeDataPlane::OnDeviceDisconnected(charm::contracts::DeviceHandle device_handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  charm::core::DetachDeviceRequest detach_request{};
  detach_request.device_handle = device_handle;
  const auto detach_result = device_registry_.DetachDevice(detach_request);
  if (detach_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("detach rejected handle=%u reason=%u",
                                  device_handle.value,
                                  detach_result.fault_code.reason);
  }

  for (auto it = interface_contexts_.begin(); it != interface_contexts_.end();) {
    if (it->second.device_handle.value == device_handle.value) {
      hid_decoder_.ResetInterfaceState(it->second.interface_handle);
      it = interface_contexts_.erase(it);
    } else {
      ++it;
    }
  }
  RebuildRuntimeBundlesLocked();
  CHARM_RUNTIME_DATA_PLANE_LOGI("device disconnected handle=%u", device_handle.value);
}

void RuntimeDataPlane::OnInterfaceDescriptorAvailable(
    const charm::ports::InterfaceDescriptorRef& interface_descriptor) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (interface_contexts_.size() >= kMaxRuntimeInterfaces) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("dropping interface handle=%u, capacity reached",
                                  interface_descriptor.interface_handle.value);
    return;
  }

  const charm::ports::ClaimInterfaceRequest claim_request{
      .device_handle = interface_descriptor.device_handle,
      .interface_number = interface_descriptor.interface_number,
  };
  const auto claim_result = usb_host_.ClaimInterface(claim_request);

  const auto resolved_interface_handle =
      claim_result.status == charm::contracts::ContractStatus::kOk
          ? claim_result.interface_handle
          : interface_descriptor.interface_handle;
  if (resolved_interface_handle.value == 0) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("claim failed for device=%u interface=%u",
                                  interface_descriptor.device_handle.value,
                                  interface_descriptor.interface_number);
    return;
  }

  charm::ports::InterfaceDescriptorRef claimed_descriptor = interface_descriptor;
  claimed_descriptor.interface_handle = resolved_interface_handle;

  charm::core::RegisterInterfaceRequest register_interface_request{};
  register_interface_request.interface_descriptor = claimed_descriptor;
  const auto register_result =
      device_registry_.RegisterInterface(register_interface_request);
  if (register_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW(
        "register interface failed device=%u interface=%u reason=%u",
        claimed_descriptor.device_handle.value, claimed_descriptor.interface_number,
        register_result.fault_code.reason);
    return;
  }

  charm::core::ParseDescriptorRequest parse_request{};
  parse_request.device_handle = claimed_descriptor.device_handle;
  parse_request.interface_handle = claimed_descriptor.interface_handle;
  parse_request.interface_number = claimed_descriptor.interface_number;
  parse_request.descriptor = claimed_descriptor.descriptor;
  const auto parse_result = descriptor_parser_.ParseDescriptor(parse_request);
  if (parse_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW(
        "descriptor parse failed device=%u interface=%u bytes=%u reason=%u",
        claimed_descriptor.device_handle.value, claimed_descriptor.interface_number,
        static_cast<unsigned>(claimed_descriptor.descriptor.size),
        parse_result.fault_code.reason);
    return;
  }

  charm::core::BuildDecodePlanRequest build_request{};
  build_request.input.device_handle = claimed_descriptor.device_handle;
  build_request.input.interface_handle = claimed_descriptor.interface_handle;
  build_request.input.interface_number = claimed_descriptor.interface_number;
  charm::core::LookupDeviceRequest lookup_request{};
  lookup_request.device_handle = claimed_descriptor.device_handle;
  const auto lookup_result = device_registry_.LookupDevice(lookup_request);
  if (lookup_result.status == charm::contracts::ContractStatus::kOk) {
    build_request.input.vendor_id = lookup_result.enumeration_info.vendor_id;
    build_request.input.product_id = lookup_result.enumeration_info.product_id;
    build_request.input.hub_path = lookup_result.enumeration_info.hub_path;
  }
  build_request.input.semantic_model = parse_result.semantic_model;
  const auto build_result = decode_plan_builder_.BuildDecodePlan(build_request);
  if (build_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW(
        "decode plan build failed device=%u interface=%u reason=%u",
        claimed_descriptor.device_handle.value, claimed_descriptor.interface_number,
        build_result.fault_code.reason);
    return;
  }

  auto decode_plan = std::make_unique<charm::core::DecodePlan>(build_result.decode_plan);

  charm::core::AttachDecodePlanRequest attach_request{};
  attach_request.interface_handle = claimed_descriptor.interface_handle;
  attach_request.decode_plan.plan = decode_plan.get();
  const auto attach_result = device_registry_.AttachDecodePlan(attach_request);
  if (attach_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW(
        "attach decode plan failed device=%u interface=%u reason=%u",
        claimed_descriptor.device_handle.value, claimed_descriptor.interface_number,
        attach_result.fault_code.reason);
    return;
  }

  InterfaceContext context{};
  context.device_handle = claimed_descriptor.device_handle;
  context.interface_handle = claimed_descriptor.interface_handle;
  context.interface_number = claimed_descriptor.interface_number;
  context.decode_plan = std::move(decode_plan);
  context.compiled_bundle = {};

  hid_decoder_.ResetInterfaceState(claimed_descriptor.interface_handle);
  interface_contexts_[MakeInterfaceKey(claimed_descriptor.interface_handle)] =
      std::move(context);
  RebuildRuntimeBundlesLocked();

  CHARM_RUNTIME_DATA_PLANE_LOGI(
      "interface ready device=%u iface=%u handle=%u bindings=%u",
      claimed_descriptor.device_handle.value, claimed_descriptor.interface_number,
      claimed_descriptor.interface_handle.value,
      static_cast<unsigned>(interface_contexts_[MakeInterfaceKey(claimed_descriptor.interface_handle)]
                               .decode_plan->binding_count));
}

void RuntimeDataPlane::OnReportReceived(
    const charm::contracts::RawHidReportRef& report_ref) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto context_it =
      interface_contexts_.find(MakeInterfaceKey(report_ref.interface_handle));
  if (context_it == interface_contexts_.end() ||
      context_it->second.decode_plan == nullptr) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("dropping report: unknown interface handle=%u",
                                  report_ref.interface_handle.value);
    return;
  }

  const auto& context = context_it->second;
  std::array<charm::contracts::InputElementEvent,
             charm::core::kMaxDecodeBindingsPerInterface>
      events{};
  const auto active_bundle_result =
      mapping_bundle_loader_.GetActiveBundle(charm::core::GetActiveBundleRequest{});

  charm::core::DecodeReportRequest decode_request{};
  decode_request.report = report_ref;
  decode_request.decode_plan = context.decode_plan.get();
  decode_request.events_buffer = events.data();
  decode_request.events_buffer_capacity = events.size();
  const auto decode_result = hid_decoder_.DecodeReport(decode_request);
  if (decode_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW(
        "decode rejected interface=%u bytes=%u declared=%u reason=%u",
        report_ref.interface_handle.value, static_cast<unsigned>(report_ref.byte_length),
        report_ref.report_meta.declared_length, decode_result.fault_code.reason);
    return;
  }

  CHARM_RUNTIME_DATA_PLANE_LOGD("decoded interface=%u events=%u",
                                report_ref.interface_handle.value,
                                static_cast<unsigned>(decode_result.event_count));

  for (std::size_t i = 0; i < decode_result.event_count; ++i) {
    const auto& event = decode_result.events[i];
    bool handled = false;

    if (active_bundle_result.status == charm::contracts::ContractStatus::kOk &&
        active_bundle_result.bundle != nullptr) {
      const auto canonical_it = canonical_source_assignments_.find(
          MakeSourceKey(event.element_key_hash, event.element_type));
      if (canonical_it != canonical_source_assignments_.end()) {
        charm::core::ApplyInputEventRequest active_request{};
        active_request.input_event = event;
        active_request.input_event.element_key_hash = canonical_it->second.source;
        active_request.input_event.element_type = canonical_it->second.source_type;
        active_request.input_event.value =
            NormalizeForCompiledBundle(event, canonical_it->second.source_type);
        active_request.active_bundle_ref = active_bundle_result.bundle->bundle_ref;
        active_request.active_bundle = active_bundle_result.bundle;
        const auto active_apply_result =
            mapping_engine_.ApplyInputEvent(active_request);
        if (active_apply_result.status != charm::contracts::ContractStatus::kOk) {
          CHARM_RUNTIME_DATA_PLANE_LOGW(
              "compiled mapping apply failed interface=%u source=0x%llx reason=%u",
              report_ref.interface_handle.value,
              static_cast<unsigned long long>(event.element_key_hash.value),
              active_apply_result.fault_code.reason);
          return;
        }
        handled = active_apply_result.mapped;
      }
    }

    if (!handled) {
      charm::core::ApplyInputEventRequest apply_request{};
      apply_request.input_event = event;
      apply_request.active_bundle_ref = context.compiled_bundle.bundle_ref;
      apply_request.active_bundle = &context.compiled_bundle;
      const auto apply_result = mapping_engine_.ApplyInputEvent(apply_request);
      if (apply_result.status != charm::contracts::ContractStatus::kOk) {
        CHARM_RUNTIME_DATA_PLANE_LOGW(
            "mapping apply failed interface=%u source=0x%llx reason=%u",
            report_ref.interface_handle.value,
            static_cast<unsigned long long>(event.element_key_hash.value),
            apply_result.fault_code.reason);
        return;
      }
    }
  }

  auto supervisor_state = supervisor_.GetState();
  auto selected_profile = supervisor_state.active_profile.profile_id;
  if (selected_profile.value == 0) {
    selected_profile = kDefaultProfileId;
  }

  const auto select_profile_result = profile_manager_.SelectProfile(
      charm::contracts::SelectProfileRequest{.profile_id = selected_profile});
  if (select_profile_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("profile selection failed profile=%u reason=%u",
                                  selected_profile.value,
                                  select_profile_result.fault_code.reason);
    return;
  }

  const auto ble_profile_result =
      ble_transport_.SelectProfile({.profile_id = selected_profile});
  if (ble_profile_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("ble profile selection failed profile=%u reason=%u",
                                  selected_profile.value,
                                  ble_profile_result.fault_code.reason);
    return;
  }
  CHARM_RUNTIME_DATA_PLANE_LOGD("ble profile active profile=%u",
                                selected_profile.value);

  charm::core::GetLogicalStateRequest logical_state_request{};
  logical_state_request.profile_id = kDefaultProfileId;
  const auto logical_state_result =
      mapping_engine_.GetLogicalState(logical_state_request);
  if (logical_state_result.status != charm::contracts::ContractStatus::kOk ||
      logical_state_result.snapshot.state == nullptr) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("logical state unavailable profile=%u",
                                  selected_profile.value);
    return;
  }

  std::array<std::uint8_t, 64> encoded_buffer{};
  charm::core::EncodeLogicalStateRequest encode_request{};
  encode_request.profile_id = selected_profile;
  encode_request.logical_state = logical_state_result.snapshot.state;
  encode_request.output_buffer = encoded_buffer.data();
  encode_request.output_buffer_capacity = encoded_buffer.size();
  const auto encode_result = profile_manager_.EncodeLogicalState(encode_request);
  if (encode_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("profile encode failed profile=%u reason=%u",
                                  selected_profile.value,
                                  encode_result.fault_code.reason);
    return;
  }

  charm::ports::NotifyInputReportRequest notify_request{};
  notify_request.report = encode_result.report;
  const auto notify_result = ble_transport_.NotifyInputReport(notify_request);
  if (notify_result.status != charm::contracts::ContractStatus::kOk) {
    CHARM_RUNTIME_DATA_PLANE_LOGW("notify failed status=%u reason=%u",
                                  static_cast<unsigned>(notify_result.status),
                                  notify_result.fault_code.reason);
    return;
  }
  CHARM_RUNTIME_DATA_PLANE_LOGD("notify sent report_id=%u bytes=%u",
                                notify_request.report.report_id,
                                static_cast<unsigned>(notify_request.report.size));
}

void RuntimeDataPlane::OnStatusChanged(const charm::ports::UsbHostStatus& status) {
  CHARM_RUNTIME_DATA_PLANE_LOGI("usb status state=%u contract=%u reason=%u",
                                static_cast<unsigned>(status.state),
                                static_cast<unsigned>(status.status),
                                status.fault_code.reason);
}

std::uint32_t RuntimeDataPlane::MakeInterfaceKey(
    charm::contracts::InterfaceHandle interface_handle) {
  return interface_handle.value;
}

std::uint64_t RuntimeDataPlane::MakeSourceKey(
    charm::contracts::ElementKeyHash source,
    charm::contracts::InputElementType source_type) {
  return (static_cast<std::uint64_t>(source.value) << 8) |
         static_cast<std::uint64_t>(source_type);
}

std::uint32_t RuntimeDataPlane::StableMix(std::uint32_t seed, std::uint32_t value) {
  std::uint32_t mixed = seed ^ (value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u));
  mixed ^= (mixed << 13u);
  mixed ^= (mixed >> 17u);
  mixed ^= (mixed << 5u);
  return mixed;
}

std::uint16_t RuntimeDataPlane::SelectDeterministicIndex(
    std::size_t capacity, std::uint32_t seed, std::uint64_t source_key,
    const std::unordered_set<std::uint16_t>& used) {
  if (capacity == 0) {
    return 0;
  }
  std::uint16_t start = static_cast<std::uint16_t>(
      StableMix(seed, static_cast<std::uint32_t>(source_key & 0xffffffffu)) % capacity);
  for (std::size_t i = 0; i < capacity; ++i) {
    const auto idx = static_cast<std::uint16_t>((start + i) % capacity);
    if (used.find(idx) == used.end()) {
      return idx;
    }
  }
  return start;
}

std::unordered_map<std::uint64_t, charm::core::LogicalElementRef>
RuntimeDataPlane::ComputeTargetAssignmentsLocked(std::uint32_t bundle_seed) const {
  std::vector<SourceBinding> all_sources;
  all_sources.reserve(kMaxRuntimeSources);
  std::unordered_set<std::uint64_t> seen_source_keys{};
  seen_source_keys.reserve(kMaxRuntimeSources);

  for (const auto& context_pair : interface_contexts_) {
    const auto& context = context_pair.second;
    if (context.decode_plan == nullptr) {
      continue;
    }
    auto append_source = [&](charm::contracts::ElementKeyHash source_hash,
                             charm::contracts::InputElementType source_type) {
      if (all_sources.size() >= kMaxRuntimeSources) {
        return;
      }
      const auto unique_source_key = MakeSourceKey(source_hash, source_type);
      if (!seen_source_keys.insert(unique_source_key).second) {
        return;
      }
      all_sources.push_back(SourceBinding{
          .source = source_hash,
          .source_type = source_type,
          .device_handle = context.device_handle,
          .interface_handle = context.interface_handle,
      });
    };
    for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
      const auto& binding = context.decode_plan->bindings[i];
      if (binding.is_array && binding.has_usage_range) {
        for (std::uint32_t usage = binding.usage_min; usage <= binding.usage_max;
             ++usage) {
          if (usage == 0) {
            if (usage == binding.usage_max) {
              break;
            }
            continue;
          }
          append_source(charm::core::ComputeElementKeyHash(
                            charm::core::MakeElementKeyForUsage(
                                binding, static_cast<charm::contracts::Usage>(usage))),
                        binding.element_type);
          if (all_sources.size() >= kMaxRuntimeSources) {
            break;
          }
          if (usage == binding.usage_max) {
            break;
          }
        }
      } else {
        append_source(binding.element_key_hash, binding.element_type);
      }
      if (all_sources.size() >= kMaxRuntimeSources) {
        break;
      }
    }
  }

  std::sort(all_sources.begin(), all_sources.end(),
            [](const SourceBinding& lhs, const SourceBinding& rhs) {
              if (lhs.source.value != rhs.source.value) {
                return lhs.source.value < rhs.source.value;
              }
              if (lhs.source_type != rhs.source_type) {
                return lhs.source_type < rhs.source_type;
              }
              if (lhs.device_handle.value != rhs.device_handle.value) {
                return lhs.device_handle.value < rhs.device_handle.value;
              }
              return lhs.interface_handle.value < rhs.interface_handle.value;
            });

  std::unordered_map<std::uint64_t, charm::core::LogicalElementRef> assignments{};
  assignments.reserve(all_sources.size());
  std::unordered_set<std::uint16_t> used_axes{};
  std::unordered_set<std::uint16_t> used_buttons{};
  std::unordered_set<std::uint16_t> used_triggers{};
  std::unordered_set<std::uint16_t> used_hats{};

  for (const auto& source : all_sources) {
    const auto source_key = MakeSourceKey(source.source, source.source_type);
    if (assignments.find(source_key) != assignments.end()) {
      continue;
    }

    charm::core::LogicalElementRef target{};
    switch (source.source_type) {
      case charm::contracts::InputElementType::kAxis:
      case charm::contracts::InputElementType::kScalar: {
        if (used_axes.size() >= charm::contracts::kMaxLogicalAxes) {
          continue;
        }
        const auto idx = SelectDeterministicIndex(charm::contracts::kMaxLogicalAxes,
                                                  bundle_seed, source_key, used_axes);
        target.type = charm::core::LogicalElementType::kAxis;
        target.index = idx;
        used_axes.insert(idx);
        break;
      }
      case charm::contracts::InputElementType::kButton: {
        if (used_buttons.size() >= kMaxEncodableButtons) {
          continue;
        }
        const auto idx = SelectDeterministicIndex(
            kMaxEncodableButtons, bundle_seed ^ 0xB0u, source_key, used_buttons);
        target.type = charm::core::LogicalElementType::kButton;
        target.index = idx;
        used_buttons.insert(idx);
        break;
      }
      case charm::contracts::InputElementType::kTrigger: {
        if (used_triggers.size() < 2) {
          const auto idx = SelectDeterministicIndex(2, bundle_seed ^ 0x71u, source_key,
                                                    used_triggers);
          target.type = charm::core::LogicalElementType::kTrigger;
          target.index = idx;
          used_triggers.insert(idx);
        } else {
          if (used_axes.size() >= charm::contracts::kMaxLogicalAxes) {
            continue;
          }
          const auto idx = SelectDeterministicIndex(charm::contracts::kMaxLogicalAxes,
                                                    bundle_seed ^ 0x81u, source_key,
                                                    used_axes);
          target.type = charm::core::LogicalElementType::kAxis;
          target.index = idx;
          used_axes.insert(idx);
        }
        break;
      }
      case charm::contracts::InputElementType::kHat: {
        if (used_hats.size() < kMaxHatMappings) {
          target.type = charm::core::LogicalElementType::kHat;
          target.index = 0;
          used_hats.insert(0);
        } else {
          if (used_buttons.size() >= kMaxEncodableButtons) {
            continue;
          }
          const auto idx = SelectDeterministicIndex(kMaxEncodableButtons,
                                                    bundle_seed ^ 0xC1u, source_key,
                                                    used_buttons);
          target.type = charm::core::LogicalElementType::kButton;
          target.index = idx;
          used_buttons.insert(idx);
        }
        break;
      }
      default:
        continue;
    }
    assignments[source_key] = target;
  }
  return assignments;
}

std::unordered_map<std::uint64_t, RuntimeDataPlane::CanonicalSourceBinding>
RuntimeDataPlane::ComputeCanonicalSourceAssignmentsLocked() const {
  std::vector<SourceBinding> all_sources;
  all_sources.reserve(kMaxRuntimeSources);
  std::unordered_set<std::uint64_t> seen_source_keys{};
  seen_source_keys.reserve(kMaxRuntimeSources);

  for (const auto& context_pair : interface_contexts_) {
    const auto& context = context_pair.second;
    if (context.decode_plan == nullptr) {
      continue;
    }
    auto append_source = [&](charm::contracts::ElementKeyHash source_hash,
                             charm::contracts::InputElementType source_type) {
      if (all_sources.size() >= kMaxRuntimeSources) {
        return;
      }
      const auto source_key = MakeSourceKey(source_hash, source_type);
      if (!seen_source_keys.insert(source_key).second) {
        return;
      }
      all_sources.push_back(SourceBinding{
          .source = source_hash,
          .source_type = source_type,
          .device_handle = context.device_handle,
          .interface_handle = context.interface_handle,
      });
    };

    for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
      const auto& binding = context.decode_plan->bindings[i];
      const auto canonical_type =
          charm::core::CanonicalizeCompilerSourceType(binding.element_type);
      if (binding.is_array && binding.has_usage_range) {
        if (canonical_type != charm::contracts::InputElementType::kButton) {
          continue;
        }
        for (std::uint32_t usage = binding.usage_min; usage <= binding.usage_max;
             ++usage) {
          if (usage == 0) {
            if (usage == binding.usage_max) {
              break;
            }
            continue;
          }
          append_source(charm::core::ComputeElementKeyHash(
                            charm::core::MakeElementKeyForUsage(
                                binding, static_cast<charm::contracts::Usage>(usage))),
                        binding.element_type);
          if (usage == binding.usage_max) {
            break;
          }
        }
        continue;
      }

      if (canonical_type == charm::contracts::InputElementType::kUnknown) {
        continue;
      }
      append_source(binding.element_key_hash, binding.element_type);
    }
  }

  std::sort(all_sources.begin(), all_sources.end(),
            [](const SourceBinding& lhs, const SourceBinding& rhs) {
              if (lhs.source.value != rhs.source.value) {
                return lhs.source.value < rhs.source.value;
              }
              if (lhs.source_type != rhs.source_type) {
                return lhs.source_type < rhs.source_type;
              }
              if (lhs.device_handle.value != rhs.device_handle.value) {
                return lhs.device_handle.value < rhs.device_handle.value;
              }
              return lhs.interface_handle.value < rhs.interface_handle.value;
            });

  std::unordered_map<std::uint64_t, CanonicalSourceBinding> assignments{};
  assignments.reserve(all_sources.size());
  std::uint16_t analog_index = 0;
  std::uint16_t button_index = 0;

  for (const auto& source : all_sources) {
    const auto physical_source_key = MakeSourceKey(source.source, source.source_type);
    const auto canonical_type =
        charm::core::CanonicalizeCompilerSourceType(source.source_type);
    if (canonical_type == charm::contracts::InputElementType::kAxis) {
      if (analog_index >= charm::core::kMaxCompilerAnalogSources) {
        continue;
      }
      assignments[physical_source_key] = CanonicalSourceBinding{
          .source = charm::core::MakeCompilerSourceHash(canonical_type, analog_index),
          .source_type = canonical_type,
      };
      ++analog_index;
      continue;
    }
    if (canonical_type == charm::contracts::InputElementType::kButton) {
      if (button_index >= charm::core::kMaxCompilerButtonSources) {
        continue;
      }
      assignments[physical_source_key] = CanonicalSourceBinding{
          .source = charm::core::MakeCompilerSourceHash(canonical_type, button_index),
          .source_type = canonical_type,
      };
      ++button_index;
    }
  }

  return assignments;
}

void RuntimeDataPlane::RebuildRuntimeBundlesLocked() {
  const std::uint32_t seed = kDefaultBundleSeed;
  const auto assignments = ComputeTargetAssignmentsLocked(seed);
  canonical_source_assignments_ = ComputeCanonicalSourceAssignmentsLocked();
  for (auto& context_pair : interface_contexts_) {
    auto& context = context_pair.second;
    context.compiled_bundle = BuildRuntimeBundle(context, assignments, seed);
    CHARM_RUNTIME_DATA_PLANE_LOGD(
        "bundle rebuilt iface=%u entries=%u bundle_id=%u integrity=0x%08x",
        context.interface_handle.value,
        static_cast<unsigned>(context.compiled_bundle.entry_count),
        context.compiled_bundle.bundle_ref.bundle_id,
        context.compiled_bundle.bundle_ref.integrity);
  }
}

charm::core::CompiledMappingBundle RuntimeDataPlane::BuildRuntimeBundle(
    const InterfaceContext& context,
    const std::unordered_map<std::uint64_t, charm::core::LogicalElementRef>&
        assigned_targets,
    std::uint32_t bundle_seed) const {
  charm::core::CompiledMappingBundle bundle{};
  const std::uint32_t fallback_bundle_id =
      context.interface_handle.value == 0 ? 1 : context.interface_handle.value;
  bundle.bundle_ref.bundle_id = StableMix(bundle_seed, fallback_bundle_id);
  bundle.bundle_ref.version = charm::core::kSupportedMappingBundleVersion;

  if (context.decode_plan == nullptr) {
    bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
    return bundle;
  }

  std::unordered_set<std::uint64_t> added_source_keys{};
  added_source_keys.reserve(context.decode_plan->binding_count);
  auto append_entry = [&](charm::contracts::ElementKeyHash source_hash,
                          charm::contracts::InputElementType source_type) {
    if (bundle.entry_count >= charm::core::kMaxMappingEntries) {
      return;
    }
    const auto source_key = MakeSourceKey(source_hash, source_type);
    if (!added_source_keys.insert(source_key).second) {
      return;
    }
    const auto assignment_it = assigned_targets.find(source_key);
    if (assignment_it == assigned_targets.end()) {
      return;
    }

    auto& entry = bundle.entries[bundle.entry_count];
    entry.source = source_hash;
    entry.source_type = source_type;
    entry.scale = charm::core::kMappingScaleOne;
    entry.offset = 0;
    entry.target = assignment_it->second;
    entry.deadzone = 0;
    const auto [clamp_min, clamp_max] = DefaultClampRange(entry.target.type);
    entry.clamp_min = clamp_min;
    entry.clamp_max = clamp_max;
    ++bundle.entry_count;
  };

  for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
    if (bundle.entry_count >= charm::core::kMaxMappingEntries) {
      break;
    }

    const auto& binding = context.decode_plan->bindings[i];
    if (binding.is_array && binding.has_usage_range) {
      for (std::uint32_t usage = binding.usage_min; usage <= binding.usage_max;
           ++usage) {
        if (usage == 0 || bundle.entry_count >= charm::core::kMaxMappingEntries) {
          if (usage == binding.usage_max) {
            break;
          }
          continue;
        }
        append_entry(charm::core::ComputeElementKeyHash(
                         charm::core::MakeElementKeyForUsage(
                             binding, static_cast<charm::contracts::Usage>(usage))),
                     binding.element_type);
        if (usage == binding.usage_max) {
          break;
        }
      }
      continue;
    }

    if (assigned_targets.find(MakeSourceKey(binding.element_key_hash,
                                            binding.element_type)) ==
        assigned_targets.end()) {
      CHARM_RUNTIME_DATA_PLANE_LOGW(
          "unassigned source dropped iface=%u source=0x%llx type=%u",
          context.interface_handle.value,
          static_cast<unsigned long long>(binding.element_key_hash.value),
          static_cast<unsigned>(binding.element_type));
      continue;
    }
    append_entry(binding.element_key_hash, binding.element_type);
  }

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  return bundle;
}

}  // namespace charm::app
