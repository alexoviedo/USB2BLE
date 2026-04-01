#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/core/mapping_bundle.hpp"

namespace charm::core {

inline constexpr std::size_t kMaxCompileDiagnostics = 64;

enum class DiagnosticSeverity : std::uint8_t {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
};

struct CompileDiagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::kInfo};
  charm::contracts::FaultCode fault_code{};
  std::uint32_t location{0};
};

struct CompileDiagnostics {
  std::array<CompileDiagnostic, kMaxCompileDiagnostics> entries{};
  std::size_t entry_count{0};
};

struct ValidateConfigRequest {
  MappingConfigDocument document{};
};

struct ValidateConfigResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  CompileDiagnostics diagnostics{};
};

struct CompileConfigRequest {
  MappingConfigDocument document{};
};

struct CompileConfigResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  CompiledMappingBundle bundle{};
  CompileDiagnostics diagnostics{};
};

class ConfigCompiler {
 public:
  virtual ~ConfigCompiler() = default;

  virtual ValidateConfigResult ValidateConfig(const ValidateConfigRequest& request) const = 0;
  virtual CompileConfigResult CompileConfig(const CompileConfigRequest& request) const = 0;
};

}  // namespace charm::core
