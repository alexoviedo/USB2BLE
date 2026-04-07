# ARCHITECTURE.md

## Status
- State: active
- Source of truth:
  - firmware code under `components/` and `main/`
  - firmware tests under `tests/unit/`
  - web companion code under `charm-web-companion/`

## Layer Model
1. **Contracts** (`components/charm_contracts`) — shared data/transport/status models.
2. **Ports** (`components/charm_ports`) — boundary interfaces for USB host, BLE transport, storage, and time.
3. **Core** (`components/charm_core`) — deterministic decode/mapping/logical/profile/supervisor logic.
4. **App** (`components/charm_app`) — startup orchestration and config transport service/runtime adapter.
5. **Platform** (`components/charm_platform_*`) — ESP-IDF-backed implementations + host-test-safe builds.
6. **Web Companion** (`charm-web-companion`) — static browser runtime for flash, console, config, validate, and operator workflows.

## Runtime Data Path (Implemented)
- USB host events are consumed by `RuntimeDataPlane`.
- Runtime pipeline performs descriptor parse -> decode plan -> HID decode -> mapping -> profile encode -> BLE notify.
- Fail-safe behavior for malformed/unsupported input is unit-tested.

## Profile Support Status
- The implemented output-profile contract is explicitly two-profile:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- `ProfileManager`, `RuntimeDataPlane`, and `BleTransportAdapter` now carry the selected profile through the BLE-facing contract.
- Documentation and release criteria must not imply broader profile support than those two implemented profiles.
- Physical BLE evidence is still required before release claims for either profile.

## Config Transport Path

### Current implemented behavior
- `ConfigTransportService` owns `persist`, `load`, `clear`, and `get_capabilities`.
- `ConfigTransportRuntimeAdapter` performs stream framing/deframing with `@CFG:` prefix and JSON payloads.
- `InitializeAndRun()` starts the UART runtime task that feeds the adapter and emits framed responses.
- Current device-side persisted state remains narrow:
  - compiled mapping bundle bytes
  - `mapping_bundle` ref
  - `profile_id`
  - optional `bonding_material`
- The web companion Config UX exposes exactly the two implemented profiles as constrained choices.
- `config.get_capabilities` confirms the serial config contract only; it does not dynamically enumerate profile IDs.
- Raw arbitrary JSON draft persistence is not part of the current implemented contract.

## Startup/Storage
- Startup lifecycle includes explicit `nvs_flash_init` handling in storage lifecycle path.

## Validation and Release Boundary
- Hardware behavior and reliability of USB/BLE transports are not proved by host CI alone.
- PR CI now proves firmware host tests, web tests/build, and ESP-IDF firmware build, but not physical device behavior.
- Physical validation remains required before production claims.
- A release candidate is blocked until every mandatory hardware scenario in `HARDWARE_VALIDATION_PACK.md` has retained evidence.
