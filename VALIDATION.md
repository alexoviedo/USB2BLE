# VALIDATION.md

## Purpose
Define what this repository can prove automatically versus what still requires physical hardware validation.

## Automated Validation (CI-proved)

### Firmware/Core host-side
- Configure/build/run all `tests/unit` targets via CMake + CTest.
- Coverage includes core logic, runtime data-plane behavior, config transport service/runtime adapter framing, USB adapter simulation hooks, BLE adapter state/recovery behaviors, and startup storage lifecycle.

### Firmware build
- Build the ESP32-S3 firmware through GitHub Actions using ESP-IDF.
- Publish firmware artifacts and integrity metadata for traceability.

## Manual Validation (Hardware-required)
CI cannot prove these:
- Real USB host enumeration and HID behavior on the actual powered hub/device matrix.
- Real BLE pairing, reconnect, latency, and sustained runtime stability.
- End-to-end flashing and runtime behavior on physical ESP32-S3 hardware.

## Local Host-Side Validation Commands

```bash
cmake -S tests/unit -B build/unit
cmake --build build/unit --parallel
ctest --test-dir build/unit --output-on-failure
```
