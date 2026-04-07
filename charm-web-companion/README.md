# Charm Web Companion

Companion static web runtime for the Charm ESP32-S3 firmware project.

## Product Responsibilities
- Flash firmware artifacts to ESP32-S3 from the browser.
- Serial console for logs/debugging.
- Config UI for local draft authoring and device sync commands.
- Browser-side Gamepad API validation view.
- Operator guidance and troubleshooting.

## Current Firmware-Backed Truth

- The web companion is part of the shipped repository direction, not an optional side project.
- Flashing, console monitoring, and device config transport are serial-first browser workflows.
- The current implemented firmware config contract is `v2` over `@CFG:` with:
  - `config.persist`
  - `config.load`
  - `config.clear`
  - `config.get_capabilities`
- `config.persist` sends a versioned `mapping_document` over serial; firmware compiles it into a runtime-effective bundle before persisting.
- Device persistence is still intentionally narrow:
  - compiled mapping bundle bytes
  - `mapping_bundle` ref
  - `profile_id`
  - optional `bonding_material`
- The current firmware does **not** persist the full rich browser draft or return raw compiled bundle bytes to the web UI.
- The current branch exposes two firmware-backed config profile IDs in the web companion:
  - `1 = Generic BLE Gamepad`
  - `2 = Wireless Xbox Controller`
- `config.get_capabilities` still verifies the serial config contract only; it does not dynamically enumerate those profile IDs.
- PR CI enforces the web quality lane with `npx vitest run` and `npm run build` under Node `20.19.4`.
- Web lint remains intentionally deferred until `2026-04-20` because the current ESLint patch path is unstable under ESLint 9.
- Current retained hardware browser proof is mixed:
  - `Capabilities` succeeds on the current flashed image
  - `Persist`, `Clear`, and dependent `Load` steps are not yet hardware-proven and currently fail in retained evidence

## Requirements
- **Secure Context**: HTTPS or localhost required for Web Serial.
- **Supported Browser**: Desktop Chromium-based browser (Chrome, Edge, Opera, Brave).
- **Gamepad API**: Required for full Validate view behavior.

## Local Development

1.  **Install dependencies**:
    ```bash
    npm install
    ```
2.  **Start development server**:
    ```bash
    npm run dev
    ```
3.  **Run tests**:
    ```bash
    PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run
    ```

## Build & Static Deployment

The webapp is automatically deployed to GitHub Pages at: [https://alexoviedo.github.io/USB2BLE/](https://alexoviedo.github.io/USB2BLE/)

1.  **Generate static build**:
    ```bash
    PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build
    ```
    The output will be in the `out/` directory (standard Next.js static export).

2.  **Deployment**:
    Serve the contents of the `out/` directory over HTTPS.

## Firmware Artifact Placement

In "Same-site manifest" mode, artifacts must be placed in a `firmware/` directory relative to the web root:
- `firmware/manifest.json`
- `firmware/bootloader.bin`
- `firmware/partition-table.bin`
- `firmware/charm.bin`

## Truth Boundaries
- **Configuration**: The browser owns the rich draft. Persist sends a versioned `mapping_document`, firmware compiles it into a runtime-effective bundle, and the device stores only the compiled bundle plus bundle/profile metadata.
- **Compiler boundary**: The firmware compiler supports the documented `mapping_document` contract only. The UI must not imply raw draft persistence or undisclosed firmware-side editing semantics.
- **Validation**: The Validate view shows browser Gamepad API data. It is not an internal BLE or firmware state inspector.
- **Transport**: Any future config expansion must remain serial-first unless a stricter requirement is proved and documented.

## Hardware Release Checklist

See `../HARDWARE_VALIDATION_PACK.md` for the full evidence pack. At a minimum, release claims must be backed by retained evidence for:

- [ ] **Environment**: App reports "Environment fully supported" in the green top banner.
- [ ] **Flash**: Identification (Chip/MAC) works; flashing completes to 100% and resets the device.
- [ ] **Console**: Connects at 115200 baud; logs stream correctly; auto-scroll works.
- [ ] **Config (Local)**: Can create/edit/delete mappings; import/export JSON works.
- [ ] **Config (Current Device Contract)**: `Get Capabilities` reports protocol `v2`; `Persist` compiles a `mapping_document`, `Load` returns persisted bundle/profile metadata, and `Clear` resets persisted config cleanly.
- [ ] **Profiles (Implemented, Hardware-Proven)**: `Generic BLE Gamepad` and `Wireless Xbox Controller` hardware evidence is captured before any release that claims them.
- [ ] **Validate**: Active controller shows live axis/button updates; neutral bars stay gray at rest.
- [ ] **Ownership**: Flash is blocked if Console is active; Console is blocked if Flash is active.
- [ ] **Release Gate**: All mandatory evidence in `HARDWARE_VALIDATION_PACK.md` exists before release sign-off.
