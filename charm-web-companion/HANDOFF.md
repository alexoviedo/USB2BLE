# Charm Web Companion - Repository Handoff Snapshot

## 1. Architecture Summary
The application is a pure static Next.js runtime built for firmware-adjacent hardware workflows.

- **Frontend**: React, TypeScript, Tailwind CSS.
- **State Management**: Zustand for environment capabilities and serial ownership.
- **Communication Layer**: Adapter-based abstractions for Web Serial and Gamepad APIs.
- **Validation**: Zod-backed schema validation for manifests and config envelopes.
- **Deployment**: Static site with no backend, database, or authentication layer.

## 2. Current Shipped Scope
- **Firmware Flashing**:
  - targets ESP32-S3
  - supports same-site manifest (`./firmware/`) and manual local import
  - provides chip identification and progress reporting
- **Serial Console**:
  - 115200 baud monitor with auto-scroll and clear
  - explicit reconnect and disconnect handling
- **Configuration**:
  - browser-local draft editing
  - device sync for the current implemented serial-first config contract
  - `@CFG:` line framing over Web Serial
- **Validation**:
  - browser Gamepad API readout
  - neutral-zone guidance and operator feedback

## 3. Current Firmware-Backed Truth
- `config.persist` now sends a versioned `mapping_document` over `@CFG:`.
- Firmware compiles that document into a persisted runtime-effective bundle.
- The device persists:
  - compiled bundle bytes
  - `mapping_bundle` ref
  - `profile_id`
  - optional `bonding_material`
- The webapp must not imply full raw JSON draft persistence on-device.
- BLE configuration transport is not supported in current firmware.
- Serial ownership is mutually exclusive across `flash`, `console`, and `config`.

## 4. Active Production Lane
- Keep firmware and web companion aligned as a single shipped product direction.
- Retain serial-first `@CFG:` as the config carrier unless a stricter requirement proves otherwise.
- Keep compiler-backed config/apply aligned with the documented `mapping_document` contract without claiming raw-draft persistence unless firmware really implements it.
- Ship the implemented two-profile surface consistently across firmware and web:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- Close release with retained hardware evidence rather than relying on automated proof alone.

## 5. Validation and Release Expectations
- Vitest covers artifact ingestion, serial lifecycle behavior, config transport framing, and top-level rendering behavior.
- Hardware validation is still mandatory for release claims.
- A release is blocked until every mandatory scenario in `../HARDWARE_VALIDATION_PACK.md` has retained evidence.
- Current retained browser truth on real hardware is mixed:
  - `Capabilities` is now proven on the current flashed image
  - `Persist`, `Clear`, and the dependent `Load` steps are still failing in retained evidence

## 6. Known Boundaries
- **No BLE config transport** in the current implementation.
- **No raw JSON persistence on-device** in the current implementation.
- **No mobile support** without Web Serial support.
- **No fake capability claims** in UI copy or operator guidance.
- **No release claim without retained hardware evidence** for the mandatory scenarios in `../HARDWARE_VALIDATION_PACK.md`.
