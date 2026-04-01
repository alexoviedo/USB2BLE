#include "charm/app/runtime_data_plane.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_set>

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

}  // namespace

RuntimeDataPlane::RuntimeDataPlane(charm::ports::UsbHostPort& usb_host,
                                   charm::ports::BleTransportPort& ble_transport,
                                   charm::core::DeviceRegistry& device_registry,
                                   charm::core::HidDescriptorParser& descriptor_parser,
                                   charm::core::DecodePlanBuilder& decode_plan_builder,
                                   charm::core::HidDecoder& hid_decoder,
                                   charm::core::MappingEngine& mapping_engine,
                                   charm::core::ProfileManager& profile_manager,
                                   charm::core::Supervisor& supervisor)
    : usb_host_(usb_host),
      ble_transport_(ble_transport),
      device_registry_(device_registry),
      descriptor_parser_(descriptor_parser),
      decode_plan_builder_(decode_plan_builder),
      hid_decoder_(hid_decoder),
      mapping_engine_(mapping_engine),
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
  context.decoded_sources.reserve(context.decode_plan->binding_count);
  for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
    context.decoded_sources.push_back(
        context.decode_plan->bindings[i].element_key_hash);
  }
  context.compiled_bundle = {};

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
    charm::core::ApplyInputEventRequest apply_request{};
    apply_request.input_event = decode_result.events[i];
    apply_request.active_bundle_ref = context.compiled_bundle.bundle_ref;
    apply_request.active_bundle = &context.compiled_bundle;
    const auto apply_result = mapping_engine_.ApplyInputEvent(apply_request);
    if (apply_result.status != charm::contracts::ContractStatus::kOk) {
      CHARM_RUNTIME_DATA_PLANE_LOGW(
          "mapping apply failed interface=%u source=0x%llx reason=%u",
          report_ref.interface_handle.value,
          static_cast<unsigned long long>(decode_result.events[i].element_key_hash.value),
          apply_result.fault_code.reason);
      return;
    }
  }

  auto supervisor_state = supervisor_.GetState();
  auto selected_profile = supervisor_state.active_profile.profile_id;
  if (selected_profile.value == 0) {
    selected_profile = kDefaultProfileId;
  }

  (void)profile_manager_.SelectProfile(
      charm::contracts::SelectProfileRequest{.profile_id = selected_profile});

  charm::core::GetLogicalStateRequest logical_state_request{};
  logical_state_request.profile_id = selected_profile;
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

  for (const auto& context_pair : interface_contexts_) {
    const auto& context = context_pair.second;
    if (context.decode_plan == nullptr) {
      continue;
    }
    for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
      const auto& binding = context.decode_plan->bindings[i];
      all_sources.push_back(SourceBinding{
          .source = binding.element_key_hash,
          .source_type = binding.element_type,
          .device_handle = context.device_handle,
          .interface_handle = context.interface_handle,
      });
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
        const auto idx = SelectDeterministicIndex(charm::contracts::kMaxLogicalAxes,
                                                  bundle_seed, source_key, used_axes);
        target.type = charm::core::LogicalElementType::kAxis;
        target.index = idx;
        used_axes.insert(idx);
        break;
      }
      case charm::contracts::InputElementType::kButton: {
        const auto idx = SelectDeterministicIndex(
            charm::contracts::kMaxLogicalButtons, bundle_seed ^ 0xB0u, source_key,
            used_buttons);
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
          const auto idx = SelectDeterministicIndex(
              charm::contracts::kMaxLogicalButtons, bundle_seed ^ 0xC1u, source_key,
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

void RuntimeDataPlane::RebuildRuntimeBundlesLocked() {
  auto supervisor_state = supervisor_.GetState();
  const auto configured_bundle = supervisor_state.active_mapping_bundle.mapping_bundle;
  const std::uint32_t seed =
      configured_bundle.bundle_id == 0
          ? kDefaultBundleSeed
          : static_cast<std::uint32_t>(configured_bundle.bundle_id ^
                                       configured_bundle.integrity ^
                                       configured_bundle.version);

  const auto assignments = ComputeTargetAssignmentsLocked(seed);
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

  for (std::size_t i = 0; i < context.decode_plan->binding_count; ++i) {
    if (bundle.entry_count >= charm::core::kMaxMappingEntries) {
      break;
    }

    const auto& binding = context.decode_plan->bindings[i];
    const auto assignment_it =
        assigned_targets.find(MakeSourceKey(binding.element_key_hash, binding.element_type));
    if (assignment_it == assigned_targets.end()) {
      CHARM_RUNTIME_DATA_PLANE_LOGW(
          "unassigned source dropped iface=%u source=0x%llx type=%u",
          context.interface_handle.value,
          static_cast<unsigned long long>(binding.element_key_hash.value),
          static_cast<unsigned>(binding.element_type));
      continue;
    }

    auto& entry = bundle.entries[bundle.entry_count];

    entry.source = binding.element_key_hash;
    entry.source_type = binding.element_type;
    entry.scale = 1;
    entry.offset = 0;
    entry.target = assignment_it->second;
    ++bundle.entry_count;
  }

  bundle.bundle_ref.integrity = charm::core::ComputeMappingBundleHash(bundle);
  return bundle;
}

}  // namespace charm::app
