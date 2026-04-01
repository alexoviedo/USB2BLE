#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "charm/contracts/error_types.hpp"
#include "charm/contracts/events.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/status_types.hpp"

namespace charm::contracts {

inline constexpr std::size_t kMaxLogicalAxes = 8;
inline constexpr std::size_t kMaxLogicalButtons = 32;

struct AxisState {
  std::int32_t value{0};
};

struct ButtonState {
  bool pressed{false};
};

struct TriggerState {
  std::uint16_t value{0};
};

struct HatState {
  std::uint8_t value{0};
};

struct LogicalGamepadState {
  std::array<AxisState, kMaxLogicalAxes> axes{};
  std::array<ButtonState, kMaxLogicalButtons> buttons{};
  TriggerState left_trigger{};
  TriggerState right_trigger{};
  HatState hat{};
};

}  // namespace charm::contracts

namespace charm::core {

struct GetLogicalStateRequest {
  charm::contracts::ProfileId profile_id{};
};

struct GetLogicalStateResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
  charm::contracts::LogicalStateSnapshot snapshot{};
};

struct ResetLogicalStateRequest {};

struct ResetLogicalStateResult {
  charm::contracts::ContractStatus status{charm::contracts::ContractStatus::kUnspecified};
  charm::contracts::FaultCode fault_code{};
};

class LogicalStateStore {
 public:
  virtual ~LogicalStateStore() = default;

  virtual GetLogicalStateResult GetLogicalState(const GetLogicalStateRequest& request) const = 0;
  virtual ResetLogicalStateResult ResetLogicalState(const ResetLogicalStateRequest& request) = 0;
};

class CanonicalLogicalStateStore final : public LogicalStateStore {
 public:
  explicit CanonicalLogicalStateStore(charm::contracts::ProfileId profile_id);
  ~CanonicalLogicalStateStore() override = default;

  GetLogicalStateResult GetLogicalState(const GetLogicalStateRequest& request) const override;
  ResetLogicalStateResult ResetLogicalState(const ResetLogicalStateRequest& request) override;

  // Mutable access for the mapping engine
  charm::contracts::LogicalGamepadState& GetMutableState(charm::contracts::Timestamp current_time);

 private:
  charm::contracts::LogicalGamepadState state_{};
  charm::contracts::ProfileId profile_id_{};
  charm::contracts::Timestamp last_update_time_{};
};

}  // namespace charm::core
