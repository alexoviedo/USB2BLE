# ARCHITECTURE.md

## Status
- State: active
- Source of truth: code under `components/`, `main/`, and tests in `tests/unit/`

## Layer Model
1. **Contracts** (`components/charm_contracts`) — shared data/transport/status models.
2. **Ports** (`components/charm_ports`) — boundary interfaces for USB host, BLE transport, storage, and time.
3. **Core** (`components/charm_core`) — deterministic decode/mapping/logical/profile/supervisor logic.
4. **App** (`components/charm_app`) — startup orchestration and config transport service/runtime adapter.
5. **Platform** (`components/charm_platform_*`) — ESP-IDF-backed implementations + host-test-safe builds.

## Runtime Data Path (Implemented)
- USB host events are consumed by `RuntimeDataPlane`.
- Runtime pipeline performs descriptor parse -> decode plan -> HID decode -> mapping -> profile encode -> BLE notify.
- Fail-safe behavior for malformed/unsupported input is unit-tested.

## Config Transport Path (Implemented)
- `ConfigTransportService` owns command semantics (`persist`, `load`, `clear`, `get_capabilities`).
- `ConfigTransportRuntimeAdapter` performs stream framing/deframing with `@CFG:` prefix and JSON payloads.
- `InitializeAndRun()` starts UART runtime task that feeds adapter and emits framed responses.

## Startup/Storage
- Startup lifecycle includes explicit `nvs_flash_init` handling in storage lifecycle path.

## What remains outside architecture proof
- Hardware behavior and reliability of USB/BLE transports are not proved by host CI alone.
- Physical validation remains required before production claims.
