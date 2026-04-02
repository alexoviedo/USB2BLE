# Charm

Charm is an ESP32-S3 firmware project for:
- USB HID input ingestion on device
- descriptor-driven decode and mapping
- BLE HID report output
- serial-framed config transport (`@CFG:`)

## Current Truth

What is implemented in this repository:
- Firmware runtime wiring exists from USB host listener to BLE notify via `RuntimeDataPlane`.
- ESP-IDF-backed USB host and BLE adapter implementations are present in platform adapters.
- Startup storage lifecycle includes explicit NVS initialization handling.
- Config transport runtime adapter is integrated in app bootstrap and uses `@CFG:` line framing to coexist with normal logs.
- Host-side unit tests cover the pure/core and adapter-safe logic under `tests/unit/`.

What is not proven by host CI alone:
- Physical USB hardware behavior on the target hub/device matrix.
- Physical BLE pairing/reconnect/runtime stability under sustained load.
- End-to-end flashing and operator workflows against real boards.

## Repo Entry Points

- Firmware app/runtime: `components/charm_app/`
- USB adapter: `components/charm_platform_usb/`
- BLE adapter: `components/charm_platform_ble/`
- Unit tests: `tests/unit/`
- CI workflows: `.github/workflows/`

## Quick Validation Commands

```bash
cmake -S tests/unit -B build/unit
cmake --build build/unit --parallel
ctest --test-dir build/unit --output-on-failure
```

## Firmware Build in GitHub Actions

GitHub Actions is configured to:
- build and run the host-side unit-test suite
- build the ESP32-S3 firmware with ESP-IDF
- publish firmware artifacts (`bootloader.bin`, `partition-table.bin`, `charm.bin`) plus integrity metadata
- deploy the Charm Web Companion to GitHub Pages: [https://alexoviedo.github.io/USB2BLE/](https://alexoviedo.github.io/USB2BLE/)
