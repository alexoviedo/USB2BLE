# Charm

Charm is a combined firmware + web companion repository for an ESP32-S3 USB-to-BLE gamepad pipeline.

## Repository Scope

Both product surfaces are in scope in this repository and must stay aligned:

- ESP32-S3 firmware:
  - USB HID ingestion on-device
  - descriptor-driven decode and mapping
  - profile encode and BLE HID output
  - serial-framed config transport over `@CFG:`
- Charm Web Companion:
  - browser-based flashing
  - serial console
  - config drafting and device-sync UX
  - browser-side validation and operator guidance

## Current Implemented Truth

What is implemented today:

- Firmware runtime wiring exists from USB host listener to BLE notify via `RuntimeDataPlane`.
- ESP-IDF-backed USB host and BLE adapter implementations are present in platform adapters.
- Startup storage lifecycle includes explicit NVS initialization handling.
- Config transport runtime adapter is integrated in app bootstrap and uses `@CFG:` line framing to coexist with normal logs.
- The current implemented config contract is serial-first `v2` with:
  - `config.persist`
  - `config.load`
  - `config.clear`
  - `config.get_capabilities`
- `config.persist` now carries a versioned `mapping_document`; firmware compiles it into a runtime-effective persisted bundle instead of treating config as browser-local only.
- Device persistence today is still intentionally narrow and honest:
  - compiled mapping bundle bytes
  - `mapping_bundle` ref derived from compiled-bundle integrity
  - `profile_id`
  - optional `bonding_material`
- Firmware output support is implemented for:
  - `profile_id = 1` `Generic BLE Gamepad`
  - `profile_id = 2` `Wireless Xbox Controller`
- The web companion Config UX exposes those same two profile IDs as constrained choices and keeps `config.get_capabilities` honest as a serial-contract check, not a profile registry.
- PR CI enforces:
  - firmware host tests
  - web Vitest
  - web production build
  - ESP-IDF firmware build
- Web lint is intentionally deferred until `2026-04-20` because the current `@rushstack/eslint-patch` path fails before repository lint rules execute under ESLint 9.
- The static Charm Web Companion is part of the shipped repository direction and is deployed to GitHub Pages.

What is not proven by host CI alone:

- Physical USB hardware behavior on the target hub/device matrix.
- Physical BLE pairing, reconnect, and runtime stability on real hosts/peers.
- End-to-end flashing, console, config, and operator workflows against real boards.

## Current Retained Hardware Evidence Status

The current branch is not at "no hardware evidence" anymore, but it is also not release-ready.

What is now retained and reproducible on the current flashed image:

- shell-side `config.get_capabilities` transport proof
- browser `Capabilities` success
- transport-binding proof showing the runtime is wired to `UART_NUM_0`

What is still not proven and currently failing in retained evidence:

- browser `config.persist`
- browser `config.clear`
- the dependent browser `config.load` steps after failed mutate operations
- the remaining mandatory USB/BLE/OPS/STG/REL scenarios in `HARDWARE_VALIDATION_PACK.md`

Current evidence entry points:

- [CFG_TRANSPORT_BINDING_AUDIT.md](/Users/alex/Developer/CodexUSB/USB2BLE/CFG_TRANSPORT_BINDING_AUDIT.md)
- [CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md](/Users/alex/Developer/CodexUSB/USB2BLE/CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md)
- [browser-roundtrip-proof.md](/Users/alex/Developer/CodexUSB/USB2BLE/browser-roundtrip-proof.md)

## Repo Entry Points

- Firmware app/runtime: `components/charm_app/`
- Firmware core/contracts/ports: `components/charm_core/`, `components/charm_contracts/`, `components/charm_ports/`
- USB adapter: `components/charm_platform_usb/`
- BLE adapter: `components/charm_platform_ble/`
- Web companion: `charm-web-companion/`
- Firmware unit tests: `tests/unit/`
- CI workflows: `.github/workflows/`

## Quick Validation Commands

Firmware host-side:

```bash
cmake -S tests/unit -B build/unit
cmake --build build/unit --parallel
ctest --test-dir build/unit --output-on-failure
```

Web companion:

```bash
cd charm-web-companion
PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run
PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build
```

Firmware image build:

```bash
source /Users/alex/esp/esp-idf/export.sh
idf.py build
```

## Release Gate

A release candidate is not shippable just because CI builds artifacts.

Release requires all of the following:

- the expected automated checks are green for the firmware and web companion
- retained hardware evidence exists for every mandatory scenario in `HARDWARE_VALIDATION_PACK.md`
- artifact checksum and provenance verification pass

## GitHub Actions

GitHub Actions is configured to:

- build and run the host-side firmware unit-test suite
- build and validate the web companion in PR CI with `npx vitest run` and `npm run build`
- build the ESP32-S3 firmware with ESP-IDF
- emit a CI summary that reports firmware host-test, web-quality, and firmware-build lane status
- publish firmware artifacts (`bootloader.bin`, `partition-table.bin`, `charm.bin`) plus integrity metadata
- build and deploy the Charm Web Companion to GitHub Pages: [https://alexoviedo.github.io/USB2BLE/](https://alexoviedo.github.io/USB2BLE/)
