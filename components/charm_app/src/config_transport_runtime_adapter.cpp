#include "charm/app/config_transport_runtime_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace charm::app {

namespace {

constexpr std::uint32_t kParserRejectProtocolVersion =
    ConfigTransportService::kProtocolVersion;
constexpr std::uint32_t kParserRejectRequestId = 0;
constexpr std::uint32_t kParserRejectIntegrity = 0;
constexpr std::uint32_t kReasonFrameTooLarge = 100;
constexpr std::uint32_t kReasonMalformedFrame = 101;
constexpr std::string_view kControlPrefix = "@CFG:";

charm::contracts::ConfigTransportResponse MakeParserRejection(
    charm::contracts::ErrorCategory category, std::uint32_t reason) {
  charm::contracts::ConfigTransportResponse response{};
  response.protocol_version = kParserRejectProtocolVersion;
  response.request_id = kParserRejectRequestId;
  response.command = charm::contracts::ConfigTransportCommand::kGetCapabilities;
  response.status = charm::contracts::ContractStatus::kRejected;
  response.fault_code.category = category;
  response.fault_code.reason = reason;
  return response;
}

}  // namespace

ConfigTransportRuntimeAdapter::ConfigTransportRuntimeAdapter(
    const ConfigTransportService& service)
    : service_(service) {}

void ConfigTransportRuntimeAdapter::ConsumeBytes(const std::uint8_t* bytes,
                                                 std::size_t size) {
  if (bytes == nullptr || size == 0) {
    return;
  }

  input_buffer_.append(reinterpret_cast<const char*>(bytes), size);

  while (true) {
    const auto line_end = input_buffer_.find('\n');
    if (line_end == std::string::npos) {
      if (input_buffer_.size() > kMaxFrameBytes) {
        output_frames_.push_back(SerializeResponseFrame(
            MakeParserRejection(charm::contracts::ErrorCategory::kCapacityExceeded,
                                kReasonFrameTooLarge)));
        input_buffer_.clear();
      }
      return;
    }

    std::string frame = input_buffer_.substr(0, line_end);
    input_buffer_.erase(0, line_end + 1);

    if (frame.size() > kMaxFrameBytes) {
      output_frames_.push_back(SerializeResponseFrame(
          MakeParserRejection(charm::contracts::ErrorCategory::kCapacityExceeded,
                              kReasonFrameTooLarge)));
      continue;
    }

    auto payload_begin = SkipWhitespace(frame, 0);
    if (payload_begin >= frame.size()) {
      continue;
    }
    if (frame.compare(payload_begin, kControlPrefix.size(), kControlPrefix) != 0) {
      // Not a config/control frame. Ignore so logs/noise can coexist on the same stream.
      continue;
    }
    payload_begin += kControlPrefix.size();
    const auto payload = frame.substr(payload_begin);
    const auto response = HandleFrame(payload);
    output_frames_.push_back(SerializeResponseFrame(response));
  }
}

bool ConfigTransportRuntimeAdapter::WritePendingFrame(
    SerialTransportWriter& writer) {
  if (output_frames_.empty()) {
    return false;
  }

  const std::string frame = output_frames_.front();
  if (!writer.Write(reinterpret_cast<const std::uint8_t*>(frame.data()),
                    frame.size())) {
    return false;
  }

  output_frames_.erase(output_frames_.begin());
  return true;
}

bool ConfigTransportRuntimeAdapter::HasPendingFrame() const {
  return !output_frames_.empty();
}

charm::contracts::ConfigTransportResponse ConfigTransportRuntimeAdapter::HandleFrame(
    const std::string& frame) const {
  charm::contracts::ConfigTransportRequest request{};
  std::vector<std::uint8_t> bonding_material_storage;

  if (!ParseRequestFrame(frame, &request, &bonding_material_storage)) {
    return MakeParserRejection(charm::contracts::ErrorCategory::kInvalidRequest,
                               kReasonMalformedFrame);
  }

  if (!bonding_material_storage.empty()) {
    request.bonding_material = bonding_material_storage.data();
    request.bonding_material_size = bonding_material_storage.size();
  }

  return service_.HandleRequest(request);
}

bool ConfigTransportRuntimeAdapter::ParseRequestFrame(
    const std::string& frame, charm::contracts::ConfigTransportRequest* request,
    std::vector<std::uint8_t>* bonding_material_storage) {
  if (request == nullptr || bonding_material_storage == nullptr) {
    return false;
  }

  std::uint32_t protocol_version = 0;
  std::uint32_t request_id = 0;
  std::string command;
  std::string integrity;

  if (!ExtractUInt(frame, "protocol_version", &protocol_version) ||
      !ExtractUInt(frame, "request_id", &request_id) ||
      !ExtractString(frame, "command", &command)) {
    return false;
  }

  const auto integrity_key_pos = frame.rfind("\"integrity\"");
  if (integrity_key_pos == std::string::npos) {
    return false;
  }
  const auto integrity_colon = frame.find(':', integrity_key_pos);
  if (integrity_colon == std::string::npos) {
    return false;
  }
  auto integrity_begin = SkipWhitespace(frame, integrity_colon + 1);
  if (integrity_begin >= frame.size() || frame[integrity_begin] != '"') {
    return false;
  }
  ++integrity_begin;
  const auto integrity_end = frame.find('"', integrity_begin);
  if (integrity_end == std::string::npos) {
    return false;
  }
  integrity = frame.substr(integrity_begin, integrity_end - integrity_begin);

  charm::contracts::ConfigTransportCommand parsed_command{};
  if (!ParseCommand(command, &parsed_command)) {
    return false;
  }

  request->protocol_version = protocol_version;
  request->request_id = request_id;
  request->command = parsed_command;
  request->integrity =
      integrity == "CFG1" ? ConfigTransportService::kExpectedIntegrity : 0;

  if (parsed_command == charm::contracts::ConfigTransportCommand::kPersist) {
    std::string payload_json;
    std::string bundle_json;
    std::uint32_t bundle_id = 0;
    std::uint32_t version = 0;
    std::uint32_t bundle_integrity = 0;
    std::uint32_t profile_id = 0;

    if (!ExtractObject(frame, "payload", &payload_json) ||
        !ExtractObject(payload_json, "mapping_bundle", &bundle_json) ||
        !ExtractUInt(bundle_json, "bundle_id", &bundle_id) ||
        !ExtractUInt(bundle_json, "version", &version) ||
        !ExtractUInt(bundle_json, "integrity", &bundle_integrity) ||
        !ExtractUInt(payload_json, "profile_id", &profile_id)) {
      return false;
    }

    request->mapping_bundle.bundle_id = bundle_id;
    request->mapping_bundle.version = version;
    request->mapping_bundle.integrity = bundle_integrity;
    request->profile_id.value = profile_id;

    if (ExtractUIntArray(payload_json, "bonding_material",
                         bonding_material_storage)) {
      request->bonding_material = bonding_material_storage->data();
      request->bonding_material_size = bonding_material_storage->size();
    }
  }

  return true;
}

std::string ConfigTransportRuntimeAdapter::SerializeResponseFrame(
    const charm::contracts::ConfigTransportResponse& response) {
  std::ostringstream out;
  out << kControlPrefix;
  out << '{';
  out << "\"protocol_version\":" << response.protocol_version << ',';
  out << "\"request_id\":" << response.request_id << ',';
  out << "\"command\":\"";
  switch (response.command) {
    case charm::contracts::ConfigTransportCommand::kPersist:
      out << "config.persist";
      break;
    case charm::contracts::ConfigTransportCommand::kLoad:
      out << "config.load";
      break;
    case charm::contracts::ConfigTransportCommand::kClear:
      out << "config.clear";
      break;
    case charm::contracts::ConfigTransportCommand::kGetCapabilities:
      out << "config.get_capabilities";
      break;
  }
  out << "\",";
  out << "\"status\":\"" << StatusToString(response.status) << "\",";
  out << "\"fault\":{";
  out << "\"category\":\""
      << ErrorCategoryToString(response.fault_code.category) << "\",";
  out << "\"reason\":" << response.fault_code.reason << "}";

  if (response.command == charm::contracts::ConfigTransportCommand::kLoad) {
    out << ",\"payload\":{";
    out << "\"mapping_bundle\":{";
    out << "\"bundle_id\":" << response.mapping_bundle.bundle_id << ',';
    out << "\"version\":" << response.mapping_bundle.version << ',';
    out << "\"integrity\":" << response.mapping_bundle.integrity << "},";
    out << "\"profile_id\":" << response.profile_id.value;
    out << '}';
  }

  if (response.command ==
      charm::contracts::ConfigTransportCommand::kGetCapabilities) {
    out << ",\"capabilities\":{";
    out << "\"protocol_version\":" << response.capabilities.protocol_version
        << ',';
    out << "\"supports_persist\":"
        << (response.capabilities.supports_persist ? "true" : "false")
        << ',';
    out << "\"supports_load\":"
        << (response.capabilities.supports_load ? "true" : "false") << ',';
    out << "\"supports_clear\":"
        << (response.capabilities.supports_clear ? "true" : "false") << ',';
    out << "\"supports_get_capabilities\":"
        << (response.capabilities.supports_get_capabilities ? "true"
                                                            : "false")
        << ',';
    out << "\"supports_ble_transport\":"
        << (response.capabilities.supports_ble_transport ? "true" : "false");
    out << '}';
  }

  out << "}\n";
  return out.str();
}

const char* ConfigTransportRuntimeAdapter::StatusToString(
    charm::contracts::ContractStatus status) {
  switch (status) {
    case charm::contracts::ContractStatus::kOk:
      return "kOk";
    case charm::contracts::ContractStatus::kRejected:
      return "kRejected";
    case charm::contracts::ContractStatus::kUnavailable:
      return "kUnavailable";
    case charm::contracts::ContractStatus::kFailed:
      return "kFailed";
    default:
      return "kUnspecified";
  }
}

const char* ConfigTransportRuntimeAdapter::ErrorCategoryToString(
    charm::contracts::ErrorCategory category) {
  switch (category) {
    case charm::contracts::ErrorCategory::kInvalidRequest:
      return "kInvalidRequest";
    case charm::contracts::ErrorCategory::kInvalidState:
      return "kInvalidState";
    case charm::contracts::ErrorCategory::kUnsupportedCapability:
      return "kUnsupportedCapability";
    case charm::contracts::ErrorCategory::kContractViolation:
      return "kContractViolation";
    case charm::contracts::ErrorCategory::kResourceExhausted:
      return "kResourceExhausted";
    case charm::contracts::ErrorCategory::kCapacityExceeded:
      return "kCapacityExceeded";
    case charm::contracts::ErrorCategory::kTimeout:
      return "kTimeout";
    case charm::contracts::ErrorCategory::kIntegrityFailure:
      return "kIntegrityFailure";
    case charm::contracts::ErrorCategory::kPersistenceFailure:
      return "kPersistenceFailure";
    case charm::contracts::ErrorCategory::kAdapterFailure:
      return "kAdapterFailure";
    case charm::contracts::ErrorCategory::kTransportFailure:
      return "kTransportFailure";
    case charm::contracts::ErrorCategory::kDeviceProtocolFailure:
      return "kDeviceProtocolFailure";
    case charm::contracts::ErrorCategory::kConfigurationRejected:
      return "kConfigurationRejected";
    case charm::contracts::ErrorCategory::kRecoveryRequired:
      return "kRecoveryRequired";
    default:
      return "kUnknown";
  }
}

bool ConfigTransportRuntimeAdapter::ExtractUInt(const std::string& json,
                                                const char* key,
                                                std::uint32_t* value) {
  if (key == nullptr || value == nullptr) {
    return false;
  }

  const std::string quoted_key = std::string("\"") + key + "\"";
  const auto key_pos = json.find(quoted_key);
  if (key_pos == std::string::npos) {
    return false;
  }

  auto colon_pos = json.find(':', key_pos + quoted_key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  auto begin = SkipWhitespace(json, colon_pos + 1);
  auto end = begin;
  while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
    ++end;
  }
  if (begin == end) {
    return false;
  }

  const auto number_text = std::string_view(json.data() + begin, end - begin);
  std::uint32_t parsed = 0;
  const auto parse_result =
      std::from_chars(number_text.data(), number_text.data() + number_text.size(),
                      parsed);
  if (parse_result.ec != std::errc()) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ConfigTransportRuntimeAdapter::ExtractString(const std::string& json,
                                                  const char* key,
                                                  std::string* value) {
  if (key == nullptr || value == nullptr) {
    return false;
  }

  const std::string quoted_key = std::string("\"") + key + "\"";
  const auto key_pos = json.find(quoted_key);
  if (key_pos == std::string::npos) {
    return false;
  }

  const auto colon_pos = json.find(':', key_pos + quoted_key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  auto begin = SkipWhitespace(json, colon_pos + 1);
  if (begin >= json.size() || json[begin] != '"') {
    return false;
  }
  ++begin;

  const auto end = json.find('"', begin);
  if (end == std::string::npos) {
    return false;
  }

  *value = json.substr(begin, end - begin);
  return true;
}

bool ConfigTransportRuntimeAdapter::ExtractObject(const std::string& json,
                                                  const char* key,
                                                  std::string* object_json) {
  if (key == nullptr || object_json == nullptr) {
    return false;
  }

  const std::string quoted_key = std::string("\"") + key + "\"";
  const auto key_pos = json.find(quoted_key);
  if (key_pos == std::string::npos) {
    return false;
  }

  const auto colon_pos = json.find(':', key_pos + quoted_key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  auto begin = SkipWhitespace(json, colon_pos + 1);
  if (begin >= json.size() || json[begin] != '{') {
    return false;
  }

  std::size_t depth = 0;
  for (std::size_t i = begin; i < json.size(); ++i) {
    if (json[i] == '{') {
      ++depth;
    } else if (json[i] == '}') {
      if (depth == 0) {
        return false;
      }
      --depth;
      if (depth == 0) {
        *object_json = json.substr(begin, i - begin + 1);
        return true;
      }
    }
  }

  return false;
}

bool ConfigTransportRuntimeAdapter::ExtractUIntArray(
    const std::string& json, const char* key, std::vector<std::uint8_t>* values) {
  if (key == nullptr || values == nullptr) {
    return false;
  }

  const std::string quoted_key = std::string("\"") + key + "\"";
  const auto key_pos = json.find(quoted_key);
  if (key_pos == std::string::npos) {
    return false;
  }

  const auto colon_pos = json.find(':', key_pos + quoted_key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  auto begin = SkipWhitespace(json, colon_pos + 1);
  if (begin >= json.size() || json[begin] != '[') {
    return false;
  }
  ++begin;

  values->clear();

  while (begin < json.size()) {
    begin = SkipWhitespace(json, begin);
    if (begin < json.size() && json[begin] == ']') {
      return true;
    }

    auto end = begin;
    while (end < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[end]))) {
      ++end;
    }

    if (begin == end) {
      return false;
    }

    std::uint32_t parsed = 0;
    const auto number_text = std::string_view(json.data() + begin, end - begin);
    const auto parse_result =
        std::from_chars(number_text.data(), number_text.data() + number_text.size(),
                        parsed);
    if (parse_result.ec != std::errc() || parsed > 255) {
      return false;
    }
    values->push_back(static_cast<std::uint8_t>(parsed));

    begin = SkipWhitespace(json, end);
    if (begin < json.size() && json[begin] == ',') {
      ++begin;
      continue;
    }
    if (begin < json.size() && json[begin] == ']') {
      return true;
    }
    return false;
  }

  return false;
}

std::size_t ConfigTransportRuntimeAdapter::SkipWhitespace(const std::string& text,
                                                          std::size_t pos) {
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  return pos;
}

bool ConfigTransportRuntimeAdapter::ParseCommand(
    const std::string& command,
    charm::contracts::ConfigTransportCommand* parsed) {
  if (parsed == nullptr) {
    return false;
  }

  if (command == "config.persist") {
    *parsed = charm::contracts::ConfigTransportCommand::kPersist;
    return true;
  }
  if (command == "config.load") {
    *parsed = charm::contracts::ConfigTransportCommand::kLoad;
    return true;
  }
  if (command == "config.clear") {
    *parsed = charm::contracts::ConfigTransportCommand::kClear;
    return true;
  }
  if (command == "config.get_capabilities") {
    *parsed = charm::contracts::ConfigTransportCommand::kGetCapabilities;
    return true;
  }

  return false;
}

}  // namespace charm::app
