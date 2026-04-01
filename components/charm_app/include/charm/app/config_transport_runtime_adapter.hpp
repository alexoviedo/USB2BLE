#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "charm/app/config_transport_service.hpp"

namespace charm::app {

class SerialTransportWriter {
 public:
  virtual ~SerialTransportWriter() = default;
  virtual bool Write(const std::uint8_t* bytes, std::size_t size) = 0;
};

class ConfigTransportRuntimeAdapter {
 public:
  explicit ConfigTransportRuntimeAdapter(const ConfigTransportService& service);

  void ConsumeBytes(const std::uint8_t* bytes, std::size_t size);
  bool WritePendingFrame(SerialTransportWriter& writer);
  bool HasPendingFrame() const;

 private:
  static constexpr std::size_t kMaxFrameBytes = 2048;

  charm::contracts::ConfigTransportResponse HandleFrame(
      const std::string& frame) const;

  static bool ParseRequestFrame(
      const std::string& frame,
      charm::contracts::ConfigTransportRequest* request,
      std::vector<std::uint8_t>* bonding_material_storage);

  static std::string SerializeResponseFrame(
      const charm::contracts::ConfigTransportResponse& response);

  static const char* StatusToString(charm::contracts::ContractStatus status);
  static const char* ErrorCategoryToString(charm::contracts::ErrorCategory category);

  static bool ExtractUInt(const std::string& json, const char* key,
                          std::uint32_t* value);
  static bool ExtractString(const std::string& json, const char* key,
                            std::string* value);
  static bool ExtractObject(const std::string& json, const char* key,
                            std::string* object_json);
  static bool ExtractUIntArray(const std::string& json, const char* key,
                               std::vector<std::uint8_t>* values);
  static std::size_t SkipWhitespace(const std::string& text, std::size_t pos);
  static bool ParseCommand(const std::string& command,
                           charm::contracts::ConfigTransportCommand* parsed);

  const ConfigTransportService& service_;
  std::string input_buffer_{};
  std::vector<std::string> output_frames_{};
};

}  // namespace charm::app
