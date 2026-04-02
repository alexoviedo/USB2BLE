# Charm Web Companion

Companion static web runtime for the Charm ESP32-S3 firmware project.

## Product Responsibilities
- Flash firmware artifacts to ESP32-S3 from the browser.
- Serial console for logs/debugging.
- Config UI for local draft authoring and device sync commands.
- Browser-side Gamepad API validation view.
- Operator guidance and troubleshooting.

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
    npx vitest run
    ```

## Build & Static Deployment

The webapp is automatically deployed to GitHub Pages at: [https://alexoviedo.github.io/USB2BLE/](https://alexoviedo.github.io/USB2BLE/)

1.  **Generate static build**:
    ```bash
    npm run build
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

## Real-Hardware Validation Checklist

- [ ] **Environment**: App reports "Environment fully supported" in the green top banner.
- [ ] **Flash**: Identification (Chip/MAC) works; flashing completes to 100% and resets the device.
- [ ] **Console**: Connects at 115200 baud; logs stream correctly; auto-scroll works.
- [ ] **Config (Local)**: Can create/edit/delete mappings; import/export JSON works.
- [ ] **Config (Device)**: `Get Capabilities` reports protocol v1; `Persist` and `Load` work for bundle refs.
- [ ] **Validate**: Active controller shows live axis/button updates; neutral bars stay gray at rest.
- [ ] **Ownership**: Flash is blocked if Console is active; Console is blocked if Flash is active.

## Truth Boundaries
- **Configuration**: The device only stores bundle references (ID, Version, Integrity) and Profile IDs. The full rich mapping draft is stored locally in the browser's `localStorage`. Always export JSON for long-term backup.
- **Validation**: The Validate view shows raw HID data from the browser. It is not an internal BLE state inspector.
