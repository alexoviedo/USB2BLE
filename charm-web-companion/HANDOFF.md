# Charm Web Companion - Final Handoff

## 1. Architecture Summary
The application is a pure static Next.js 15 runtime built for high-reliability hardware integration.

- **Frontend**: React 19, TypeScript, Tailwind CSS.
- **State Management**: Zustand (manages environment capabilities and serial ownership).
- **Communication Layer**: Adapter-based abstraction for Web Serial and Gamepad APIs.
- **Validation**: Zod (strictly enforces manifest and configuration schemas).
- **Deployment**: Static site (zero backend/DB/auth).

## 2. Supported Features
- **Firmware Flashing**:
  - Targets ESP32-S3.
  - Supports same-site manifest (./firmware/) and manual local import.
  - Automatic chip identification and deterministic progress reporting.
- **Serial Console**:
  - 115200 baud monitor with auto-scroll and clear.
  - Robust handling of port open/close and device disconnects.
- **Configuration**:
  - Local JSON draft editor for rich gamepad mappingauthoring.
  - Device persistence for mapping bundle references and profile IDs.
  - Serial transport using line-delimited `@CFG:` JSON framing.
- **Validation**:
  - Live readout of Gamepad API button and axis states.
  - Neutral-zone guidance for stick drift troubleshooting.

## 3. Unsupported / Firmware-Limited Features
- **BLE Configuration**: Not supported in current firmware; transport is serial-first.
- **Full JSON Persistence**: Firmware only stores bundle references, not the full rich draft.
- **Mobile Support**: Requires Web Serial API (currently desktop Chromium-based browsers only).
- **Multiple Simultaneous Connections**: Mutually exclusive serial ownership enforced (one tool at a time).

## 4. Testing Strategy
- **Unit Testing**: Vitest suite covers:
  - Artifact ingestion and manifest validation.
  - Serial ownership state machine.
  - Config transport wire-format and @CFG framing.
  - Environment banner messaging and view disabling.
- **Frontend Verification**: Screenshot-based visual audit for all top-level views.

## 5. Manual Validation Steps
1. **Environment Audit**: Ensure the green top banner reports "Environment fully supported".
2. **Flash Audit**: Select "Same-site manifest", click "Connect & Flash", verify chip name and MAC are identified.
3. **Console Audit**: Connect console, toggle "Auto-scroll", verify logs stream without error.
4. **Config Audit**: In "Device Sync", click "Get Capabilities", verify protocol v1 is reported.
5. **Validate Audit**: Connect controller, verify live bars move in response to stick input.

## 6. Future Enhancements (Requires Firmware Support)
- **Protocol Extension**: Implement `config.stream_full_draft` to store rich JSON drafts on-device if flash space allows.
- **BLE Transport**: Add Web Bluetooth adapter support once firmware supports BLE configuration.
- **Firmware Versioning**: Automated firmware update checks against semver manifest rules.
