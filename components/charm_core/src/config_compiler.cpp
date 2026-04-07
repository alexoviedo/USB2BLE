#include "charm/core/config_compiler.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "charm/core/decode_plan.hpp"

namespace charm::core {

namespace {

constexpr std::uint32_t kReasonEmptyDocument = 1;
constexpr std::uint32_t kReasonMalformedDocument = 2;
constexpr std::uint32_t kReasonUnsupportedDocumentVersion = 3;
constexpr std::uint32_t kReasonMissingGlobal = 4;
constexpr std::uint32_t kReasonInvalidClampRange = 5;
constexpr std::uint32_t kReasonUnknownAxisTarget = 6;
constexpr std::uint32_t kReasonUnknownButtonTarget = 7;
constexpr std::uint32_t kReasonSourceIndexOutOfRange = 8;
constexpr std::uint32_t kReasonDuplicateTarget = 9;
constexpr std::uint32_t kReasonDuplicateSource = 10;
constexpr std::uint32_t kReasonMissingAxes = 11;
constexpr std::uint32_t kReasonMissingButtons = 12;

struct AxisRule {
  std::string target{};
  std::uint32_t source_index{0};
  double scale{1.0};
  double deadzone{0.0};
  bool invert{false};
};

struct ButtonRule {
  std::string target{};
  std::uint32_t source_index{0};
};

struct ParsedDocument {
  double global_scale{1.0};
  double global_deadzone{0.0};
  double clamp_min{-1.0};
  double clamp_max{1.0};
  std::vector<AxisRule> axes{};
  std::vector<ButtonRule> buttons{};
};

void AppendDiagnostic(CompileDiagnostics* diagnostics, DiagnosticSeverity severity,
                      charm::contracts::ErrorCategory category,
                      std::uint32_t reason, std::uint32_t location) {
  if (diagnostics == nullptr ||
      diagnostics->entry_count >= diagnostics->entries.size()) {
    return;
  }
  auto& entry = diagnostics->entries[diagnostics->entry_count++];
  entry.severity = severity;
  entry.fault_code.category = category;
  entry.fault_code.reason = reason;
  entry.location = location;
}

std::size_t SkipWhitespace(std::string_view text, std::size_t pos) {
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  return pos;
}

bool ExtractDelimitedValue(std::string_view json, const char* key, char open_char,
                           char close_char, std::string* out) {
  if (key == nullptr || out == nullptr) {
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
  if (begin >= json.size() || json[begin] != open_char) {
    return false;
  }

  std::size_t depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t i = begin; i < json.size(); ++i) {
    const char ch = json[i];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }

    if (ch == '"') {
      in_string = true;
      continue;
    }
    if (ch == open_char) {
      ++depth;
      continue;
    }
    if (ch == close_char) {
      if (depth == 0) {
        return false;
      }
      --depth;
      if (depth == 0) {
        *out = std::string(json.substr(begin, i - begin + 1));
        return true;
      }
    }
  }

  return false;
}

bool ExtractObject(std::string_view json, const char* key, std::string* out) {
  return ExtractDelimitedValue(json, key, '{', '}', out);
}

bool ExtractArray(std::string_view json, const char* key, std::string* out) {
  return ExtractDelimitedValue(json, key, '[', ']', out);
}

bool ExtractString(std::string_view json, const char* key, std::string* value) {
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

  std::string parsed{};
  bool escaped = false;
  for (std::size_t i = begin; i < json.size(); ++i) {
    const char ch = json[i];
    if (escaped) {
      parsed.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      *value = parsed;
      return true;
    }
    parsed.push_back(ch);
  }
  return false;
}

bool ExtractUInt(std::string_view json, const char* key, std::uint32_t* value) {
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
  if (parse_result.ec != std::errc()) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ExtractDouble(std::string_view json, const char* key, double* value) {
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
  auto end = begin;
  if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
    ++end;
  }
  bool seen_digit = false;
  while (end < json.size()) {
    const char ch = json[end];
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      seen_digit = true;
      ++end;
      continue;
    }
    if (ch == '.' || ch == 'e' || ch == 'E' || ch == '-' || ch == '+') {
      ++end;
      continue;
    }
    break;
  }
  if (!seen_digit || begin == end) {
    return false;
  }

  const auto number_text = std::string(json.substr(begin, end - begin));
  char* parse_end = nullptr;
  const double parsed = std::strtod(number_text.c_str(), &parse_end);
  if (parse_end == number_text.c_str() ||
      static_cast<std::size_t>(parse_end - number_text.c_str()) !=
          number_text.size()) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ExtractBool(std::string_view json, const char* key, bool* value) {
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
  if (json.substr(begin, 4) == "true") {
    *value = true;
    return true;
  }
  if (json.substr(begin, 5) == "false") {
    *value = false;
    return true;
  }
  return false;
}

template <typename Callback>
bool ForEachObjectInArray(std::string_view array_json, Callback&& callback) {
  auto pos = SkipWhitespace(array_json, 0);
  if (pos >= array_json.size() || array_json[pos] != '[') {
    return false;
  }
  ++pos;
  std::size_t index = 0;

  while (true) {
    pos = SkipWhitespace(array_json, pos);
    if (pos >= array_json.size()) {
      return false;
    }
    if (array_json[pos] == ']') {
      return true;
    }
    if (array_json[pos] != '{') {
      return false;
    }

    const auto start = pos;
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (; pos < array_json.size(); ++pos) {
      const char ch = array_json[pos];
      if (in_string) {
        if (escaped) {
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == '"') {
          in_string = false;
        }
        continue;
      }
      if (ch == '"') {
        in_string = true;
        continue;
      }
      if (ch == '{') {
        ++depth;
        continue;
      }
      if (ch == '}') {
        if (depth == 0) {
          return false;
        }
        --depth;
        if (depth == 0) {
          callback(std::string(array_json.substr(start, pos - start + 1)), index++);
          ++pos;
          break;
        }
      }
    }

    pos = SkipWhitespace(array_json, pos);
    if (pos >= array_json.size()) {
      return false;
    }
    if (array_json[pos] == ',') {
      ++pos;
      continue;
    }
    if (array_json[pos] == ']') {
      return true;
    }
    return false;
  }
}

bool InRange(double value, double min_value, double max_value) {
  return value >= min_value && value <= max_value;
}

bool ParseAxisTarget(std::string_view target, std::uint16_t* index) {
  if (index == nullptr) {
    return false;
  }
  if (target == "move_x") {
    *index = 0;
    return true;
  }
  if (target == "move_y") {
    *index = 1;
    return true;
  }
  if (target == "look_x") {
    *index = 2;
    return true;
  }
  if (target == "look_y") {
    *index = 3;
    return true;
  }
  constexpr std::string_view kPrefix = "axis_";
  if (!target.starts_with(kPrefix)) {
    return false;
  }
  std::uint32_t parsed = 0;
  const auto suffix = target.substr(kPrefix.size());
  if (suffix.empty()) {
    return false;
  }
  const auto parse_result =
      std::from_chars(suffix.data(), suffix.data() + suffix.size(), parsed);
  if (parse_result.ec != std::errc() || parsed >= 4) {
    return false;
  }
  *index = static_cast<std::uint16_t>(parsed);
  return true;
}

bool ParseButtonTarget(std::string_view target, std::uint16_t* index) {
  if (index == nullptr) {
    return false;
  }
  if (target == "action_a") {
    *index = 0;
    return true;
  }
  if (target == "action_b") {
    *index = 1;
    return true;
  }
  if (target == "action_x") {
    *index = 2;
    return true;
  }
  if (target == "action_y") {
    *index = 3;
    return true;
  }
  if (target == "menu") {
    *index = 9;
    return true;
  }
  constexpr std::string_view kPrefix = "button_";
  if (!target.starts_with(kPrefix)) {
    return false;
  }
  std::uint32_t parsed = 0;
  const auto suffix = target.substr(kPrefix.size());
  if (suffix.empty()) {
    return false;
  }
  const auto parse_result =
      std::from_chars(suffix.data(), suffix.data() + suffix.size(), parsed);
  if (parse_result.ec != std::errc() || parsed >= 16) {
    return false;
  }
  *index = static_cast<std::uint16_t>(parsed);
  return true;
}

bool ParseDocument(const MappingConfigDocument& document, ParsedDocument* parsed,
                   CompileDiagnostics* diagnostics,
                   charm::contracts::FaultCode* fault_code) {
  if (parsed == nullptr || diagnostics == nullptr || fault_code == nullptr) {
    return false;
  }
  if (document.bytes == nullptr || document.size == 0) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonEmptyDocument, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonEmptyDocument;
    return false;
  }

  const std::string document_text(
      reinterpret_cast<const char*>(document.bytes), document.size);

  std::uint32_t version = 0;
  if (!ExtractUInt(document_text, "version", &version)) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMalformedDocument, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMalformedDocument;
    return false;
  }
  if (version != kSupportedMappingDocumentVersion) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kUnsupportedCapability,
                     kReasonUnsupportedDocumentVersion, version);
    fault_code->category = charm::contracts::ErrorCategory::kUnsupportedCapability;
    fault_code->reason = kReasonUnsupportedDocumentVersion;
    return false;
  }

  std::string global_json;
  if (!ExtractObject(document_text, "global", &global_json)) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMissingGlobal, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMissingGlobal;
    return false;
  }
  if (!ExtractDouble(global_json, "scale", &parsed->global_scale) ||
      !ExtractDouble(global_json, "deadzone", &parsed->global_deadzone) ||
      !ExtractDouble(global_json, "clamp_min", &parsed->clamp_min) ||
      !ExtractDouble(global_json, "clamp_max", &parsed->clamp_max)) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMissingGlobal, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMissingGlobal;
    return false;
  }

  if (!InRange(parsed->global_scale, 0.1, 4.0) ||
      !InRange(parsed->global_deadzone, 0.0, 0.95) ||
      !InRange(parsed->clamp_min, -1.0, 1.0) ||
      !InRange(parsed->clamp_max, -1.0, 1.0) ||
      parsed->clamp_min >= parsed->clamp_max) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonInvalidClampRange, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonInvalidClampRange;
    return false;
  }

  std::string axes_json;
  if (!ExtractArray(document_text, "axes", &axes_json)) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMissingAxes, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMissingAxes;
    return false;
  }

  std::string buttons_json;
  if (!ExtractArray(document_text, "buttons", &buttons_json)) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMissingButtons, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMissingButtons;
    return false;
  }

  bool axis_parse_failed = false;
  bool axis_ok = ForEachObjectInArray(axes_json,
                                      [&](const std::string& object_json,
                                          std::size_t index) {
    AxisRule rule{};
    if (!ExtractString(object_json, "target", &rule.target) ||
        !ExtractUInt(object_json, "source_index", &rule.source_index) ||
        !ExtractDouble(object_json, "scale", &rule.scale) ||
        !ExtractDouble(object_json, "deadzone", &rule.deadzone) ||
        !ExtractBool(object_json, "invert", &rule.invert)) {
      axis_parse_failed = true;
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kInvalidRequest,
                       kReasonMalformedDocument,
                       static_cast<std::uint32_t>(index));
      fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
      fault_code->reason = kReasonMalformedDocument;
      return;
    }

    if (!InRange(rule.scale, 0.1, 4.0) || !InRange(rule.deadzone, 0.0, 0.95)) {
      axis_parse_failed = true;
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kInvalidRequest,
                       kReasonMalformedDocument,
                       static_cast<std::uint32_t>(index));
      fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
      fault_code->reason = kReasonMalformedDocument;
      return;
    }

    parsed->axes.push_back(rule);
  });
  if (!axis_ok || axis_parse_failed) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMalformedDocument, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMalformedDocument;
    return false;
  }

  bool button_parse_failed = false;
  bool button_ok = ForEachObjectInArray(buttons_json,
                                        [&](const std::string& object_json,
                                            std::size_t index) {
    ButtonRule rule{};
    if (!ExtractString(object_json, "target", &rule.target) ||
        !ExtractUInt(object_json, "source_index", &rule.source_index)) {
      button_parse_failed = true;
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kInvalidRequest,
                       kReasonMalformedDocument,
                       static_cast<std::uint32_t>(index));
      fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
      fault_code->reason = kReasonMalformedDocument;
      return;
    }

    parsed->buttons.push_back(rule);
  });
  if (!button_ok || button_parse_failed) {
    AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                     charm::contracts::ErrorCategory::kInvalidRequest,
                     kReasonMalformedDocument, 0);
    fault_code->category = charm::contracts::ErrorCategory::kInvalidRequest;
    fault_code->reason = kReasonMalformedDocument;
    return false;
  }

  return true;
}

bool CompileDocument(const ParsedDocument& parsed, CompiledMappingBundle* bundle,
                     CompileDiagnostics* diagnostics,
                     charm::contracts::FaultCode* fault_code) {
  if (bundle == nullptr || diagnostics == nullptr || fault_code == nullptr) {
    return false;
  }

  std::array<bool, 4> used_axis_targets{};
  std::array<bool, 16> used_button_targets{};
  std::array<bool, kMaxCompilerAnalogSources> used_axis_sources{};
  std::array<bool, kMaxCompilerButtonSources> used_button_sources{};
  *bundle = {};
  bundle->bundle_ref.version = kSupportedMappingBundleVersion;

  auto add_axis_entry = [&](const AxisRule& rule, std::size_t location) -> bool {
    std::uint16_t target_index = 0;
    if (!ParseAxisTarget(rule.target, &target_index)) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonUnknownAxisTarget,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonUnknownAxisTarget;
      return false;
    }
    if (rule.source_index >= kMaxCompilerAnalogSources) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonSourceIndexOutOfRange,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonSourceIndexOutOfRange;
      return false;
    }
    if (used_axis_targets[target_index]) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonDuplicateTarget,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonDuplicateTarget;
      return false;
    }
    if (used_axis_sources[rule.source_index]) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonDuplicateSource,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonDuplicateSource;
      return false;
    }
    used_axis_targets[target_index] = true;
    used_axis_sources[rule.source_index] = true;

    auto& entry = bundle->entries[bundle->entry_count++];
    entry.source_type = charm::contracts::InputElementType::kAxis;
    entry.source = MakeCompilerSourceHash(entry.source_type,
                                          static_cast<std::uint16_t>(rule.source_index));
    entry.target.type = LogicalElementType::kAxis;
    entry.target.index = target_index;
    const double combined_scale =
        parsed.global_scale * rule.scale * (rule.invert ? -1.0 : 1.0);
    entry.scale = static_cast<std::int32_t>(std::lround(
        combined_scale * static_cast<double>(kMappingScaleOne)));
    entry.offset = 0;
    entry.deadzone = static_cast<std::int32_t>(std::lround(
        std::max(parsed.global_deadzone, rule.deadzone) * 127.0));
    entry.clamp_min =
        static_cast<std::int32_t>(std::lround(parsed.clamp_min * 127.0));
    entry.clamp_max =
        static_cast<std::int32_t>(std::lround(parsed.clamp_max * 127.0));
    return true;
  };

  auto add_button_entry = [&](const ButtonRule& rule, std::size_t location) -> bool {
    std::uint16_t target_index = 0;
    if (!ParseButtonTarget(rule.target, &target_index)) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonUnknownButtonTarget,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonUnknownButtonTarget;
      return false;
    }
    if (rule.source_index >= kMaxCompilerButtonSources) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonSourceIndexOutOfRange,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonSourceIndexOutOfRange;
      return false;
    }
    if (used_button_targets[target_index]) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonDuplicateTarget,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonDuplicateTarget;
      return false;
    }
    if (used_button_sources[rule.source_index]) {
      AppendDiagnostic(diagnostics, DiagnosticSeverity::kError,
                       charm::contracts::ErrorCategory::kConfigurationRejected,
                       kReasonDuplicateSource,
                       static_cast<std::uint32_t>(location));
      fault_code->category =
          charm::contracts::ErrorCategory::kConfigurationRejected;
      fault_code->reason = kReasonDuplicateSource;
      return false;
    }
    used_button_targets[target_index] = true;
    used_button_sources[rule.source_index] = true;

    auto& entry = bundle->entries[bundle->entry_count++];
    entry.source_type = charm::contracts::InputElementType::kButton;
    entry.source = MakeCompilerSourceHash(entry.source_type,
                                          static_cast<std::uint16_t>(rule.source_index));
    entry.target.type = LogicalElementType::kButton;
    entry.target.index = target_index;
    entry.scale = kMappingScaleOne;
    entry.offset = 0;
    entry.deadzone = 0;
    entry.clamp_min = 0;
    entry.clamp_max = 1;
    return true;
  };

  for (std::size_t i = 0; i < parsed.axes.size(); ++i) {
    if (!add_axis_entry(parsed.axes[i], i + 1)) {
      return false;
    }
  }
  for (std::size_t i = 0; i < parsed.buttons.size(); ++i) {
    if (!add_button_entry(parsed.buttons[i], static_cast<std::uint32_t>(100 + i))) {
      return false;
    }
  }

  std::sort(bundle->entries.begin(), bundle->entries.begin() + bundle->entry_count,
            [](const MappingEntry& lhs, const MappingEntry& rhs) {
              if (lhs.source.value != rhs.source.value) {
                return lhs.source.value < rhs.source.value;
              }
              if (lhs.source_type != rhs.source_type) {
                return lhs.source_type < rhs.source_type;
              }
              if (lhs.target.type != rhs.target.type) {
                return lhs.target.type < rhs.target.type;
              }
              return lhs.target.index < rhs.target.index;
            });

  bundle->bundle_ref.integrity = ComputeMappingBundleHash(*bundle);
  bundle->bundle_ref.bundle_id =
      bundle->bundle_ref.integrity == 0 ? 1 : bundle->bundle_ref.integrity;
  return true;
}

}  // namespace

charm::contracts::InputElementType CanonicalizeCompilerSourceType(
    charm::contracts::InputElementType source_type) {
  switch (source_type) {
    case charm::contracts::InputElementType::kAxis:
    case charm::contracts::InputElementType::kScalar:
    case charm::contracts::InputElementType::kTrigger:
      return charm::contracts::InputElementType::kAxis;
    case charm::contracts::InputElementType::kButton:
      return charm::contracts::InputElementType::kButton;
    default:
      return charm::contracts::InputElementType::kUnknown;
  }
}

charm::contracts::ElementKeyHash MakeCompilerSourceHash(
    charm::contracts::InputElementType canonical_source_type,
    std::uint16_t source_index) {
  charm::contracts::ElementKey key{};
  key.vendor_id = 0xFFFFu;
  key.product_id = 0xFFFFu;
  key.interface_number = 0xFFu;
  key.report_id = 0;
  key.usage_page = canonical_source_type ==
                           charm::contracts::InputElementType::kButton
                       ? 0xFF02u
                       : 0xFF01u;
  key.usage = source_index;
  key.collection_index = 0xFFFFu;
  key.logical_index = source_index;
  return ComputeElementKeyHash(key);
}

ValidateConfigResult DefaultConfigCompiler::ValidateConfig(
    const ValidateConfigRequest& request) const {
  ValidateConfigResult result{};
  ParsedDocument parsed{};
  if (!ParseDocument(request.document, &parsed, &result.diagnostics,
                     &result.fault_code)) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  CompiledMappingBundle bundle{};
  if (!CompileDocument(parsed, &bundle, &result.diagnostics, &result.fault_code)) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

CompileConfigResult DefaultConfigCompiler::CompileConfig(
    const CompileConfigRequest& request) const {
  CompileConfigResult result{};
  ParsedDocument parsed{};
  if (!ParseDocument(request.document, &parsed, &result.diagnostics,
                     &result.fault_code)) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  if (!CompileDocument(parsed, &result.bundle, &result.diagnostics,
                       &result.fault_code)) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

}  // namespace charm::core
