# Test Strategy

This strategy reflects the current implemented branch state on `codex/freeze-contract-plan`.

## Test Principles

1. Keep firmware contract and mapping/profile logic host-runnable wherever possible.
2. Use PR CI for the full automated baseline:
   - firmware host tests
   - web Vitest
   - web production build
   - ESP-IDF firmware build
3. Reserve hardware validation for the behaviors automation cannot credibly prove.
4. Do not widen UI or config claims beyond what firmware actually implements.
5. Treat retained hardware evidence as a release gate, not a follow-up chore.

## Current Automated Proof

### Firmware host-side
- USB ingestion and decode chain
- Mapping bundle primitives and mapping application
- Production config compiler and runtime-effective apply
- Dual-profile manager and encoder behavior
- BLE transport contract and runtime carry-through
- Config transport service/runtime adapter framing
- Startup storage lifecycle

Primary evidence:
- `tests/unit/test_hid_semantic_model.cpp`
- `tests/unit/test_decode_plan.cpp`
- `tests/unit/test_hid_decoder.cpp`
- `tests/unit/test_mapping_bundle.cpp`
- `tests/unit/test_mapping_engine.cpp`
- `tests/unit/test_config_compiler.cpp`
- `tests/unit/test_profile_manager.cpp`
- `tests/unit/test_profile_generic_gamepad_encoder.cpp`
- `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp`
- `tests/unit/test_ble_transport_adapter.cpp`
- `tests/unit/test_config_transport_service.cpp`
- `tests/unit/test_config_transport_runtime_adapter.cpp`
- `tests/unit/test_config_activation.cpp`
- `tests/unit/test_runtime_data_plane.cpp`

### Web companion
- Flash artifact ingestion and failure handling
- Console ownership/selection behavior
- Config payload/schema behavior
- Dual-profile Config UI behavior
- Rendering/environment truth-boundary coverage

Primary evidence:
- `charm-web-companion/__tests__/flash.test.ts`
- `charm-web-companion/__tests__/console.test.ts`
- `charm-web-companion/__tests__/config.test.ts`
- `charm-web-companion/__tests__/config-view.test.tsx`
- `charm-web-companion/__tests__/rendering.test.tsx`
- `charm-web-companion/__tests__/environment.test.tsx`

### CI / workflow proof
- `.github/workflows/ci.yml`
  - firmware host tests
  - web tests/build
  - ESP-IDF firmware build
- `.github/workflows/release.yml`
  - firmware preflight, firmware build, artifact packaging, integrity/provenance
- `.github/workflows/deploy-webapp.yml`
  - static web build aligned to Node `20.19.4`

## Remaining Non-Automated Proof

The following still require retained physical evidence:

- USB enumeration and multi-device stability on the real hub/device matrix
- Flash -> reboot -> console handoff on real ESP32-S3 hardware
- Compiler-backed config apply/load/reboot/reconnect on real hardware
- BLE pairing, reconnect, and live-input behavior for:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- Release checksum/provenance verification against actual candidate artifacts

Canonical scenario source:
- `HARDWARE_VALIDATION_PACK.md`

Evidence required per scenario:
- `commands.txt`
- `serial.log`
- `result.json`

## Deferred Automated Check

### Web lint
- Status: deferred until `2026-04-20`
- Reason: the current `npm run lint` path fails inside `@rushstack/eslint-patch` before repository lint rules execute under ESLint 9
- Source of truth: `CI_WEB_ENFORCEMENT_REPORT.md`

## Stop-Ship Rules

Shipping remains blocked if any of the following are true:

- Firmware host tests fail
- Web Vitest fails
- Web production build fails
- ESP-IDF firmware build fails
- Any mandatory hardware scenario in `HARDWARE_VALIDATION_PACK.md` fails
- Mandatory hardware evidence is missing
- Release checksum or provenance generation fails

## Recommended Validation Commands

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

Firmware build:

```bash
idf.py set-target esp32s3
idf.py build
```

Release integrity:

```bash
cd <artifact_dir>
sha256sum -c SHA256SUMS
cat provenance.json
```
