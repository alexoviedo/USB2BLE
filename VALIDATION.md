# VALIDATION.md

## Purpose
Define what this repository can prove automatically versus what still requires physical hardware validation.

## Current Automated Validation

### Firmware host-side
- Configure/build/run all `tests/unit` targets via CMake + CTest.
- Coverage includes core logic, runtime data-plane behavior, config transport service/runtime adapter framing, USB adapter simulation hooks, BLE adapter state/recovery behaviors, and startup storage lifecycle.

### Firmware build
- Build the ESP32-S3 firmware through GitHub Actions using ESP-IDF.
- Publish firmware artifacts and integrity metadata for traceability.

### Web companion automation status
- PR CI enforces the web companion lane with:
  - `npm ci`
  - `npx vitest run`
  - `npm run build`
- Deployment workflow uses the same Node `20.19.4` build assumption for the static export lane.
- Web lint is intentionally deferred until `2026-04-20` because the current `@rushstack/eslint-patch` path fails before repository lint rules execute under ESLint 9.

## Hardware-Required Validation

Automated testing does not prove these:

- Real USB host enumeration and HID behavior on the target powered hub/device matrix.
- Real BLE pairing, reconnect, latency, and sustained runtime stability.
- End-to-end flashing and console behavior on physical ESP32-S3 hardware.
- Compiler-backed config validation/apply flows on real hardware, including persist/load/reboot/reconnect.
- Real behavior for both implemented target output profiles:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`

## Current Retained Hardware Evidence Snapshot

As of April 7, 2026, retained hardware evidence is no longer empty, but it is not release-complete:

- transport binding is retained under `evidence/20260407/CFG-TRANSPORT-AUDIT/`
- postfix shell transport proof for `config.get_capabilities` is retained under `evidence/20260407/CFG-TRANSPORT-POSTFIX/`
- browser roundtrip evidence is retained under `evidence/20260407/CFG-BROWSER-ROUNDTRIP/`

Current truth from that retained evidence:

- shell-side `config.get_capabilities` is reproducibly passing on the current flashed image
- browser `Capabilities` is reproducibly passing on the current flashed image
- browser `Persist`, `Clear`, and dependent `Load` steps are still failing, so the config CRUD lane is not hardware-proven
- the mandatory USB/BLE/OPS/STG/REL scenarios are still outstanding

## Release Gate

A release candidate is blocked unless all of the following are true:

- the expected automated checks for firmware and web companion pass
- every mandatory scenario in `HARDWARE_VALIDATION_PACK.md` has retained evidence
- artifact checksum/provenance verification passes

Missing evidence is a release failure, not a documentation gap to fill later.

## Local Validation Commands

Firmware host-side:

```bash
cmake -S tests/unit -B build/unit
cmake --build build/unit --parallel
ctest --test-dir build/unit --output-on-failure
```

Web companion:

```bash
cd charm-web-companion
npm ci
npx vitest run
npm run build
```

Firmware image build:

```bash
source /path/to/esp-idf/export.sh
idf.py build
```

Retained hardware evidence:

```bash
scripts/hardware_evidence.sh bootstrap <date>
scripts/hardware_evidence.sh init <date> <scenario>
scripts/hardware_evidence.sh record <date> <scenario> host-precheck -- ctest --test-dir build/unit --output-on-failure
```

Capture raw device output separately to `serial.log` for the relevant scenario.
