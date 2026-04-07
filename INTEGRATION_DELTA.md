# INTEGRATION_DELTA.md

## Validation Snapshot

- Branch verified: `codex/freeze-contract-plan`
- Firmware host validation:
  - `cmake --build build/unit --parallel`
  - `ctest --test-dir build/unit --output-on-failure`
  - Result: `20/20` tests passed
- Web companion validation:
  - `PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
  - `PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`
  - Result: `5/5` test files passed, `51` tests passed, `1` skipped; production build passed
- Key implemented files present:
  - `components/charm_core/src/config_compiler.cpp`
  - `tests/unit/test_config_compiler.cpp`
  - `components/charm_core/src/profile_wireless_xbox_controller_encoder.cpp`
  - `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp`
- Retained hardware evidence root check:
  - `evidence/` directory is not present yet

## Done And Verified

### USB ingestion foundation

Done:
- Descriptor parse -> decode plan -> HID decode pipeline exists and is covered.
- Target-class ingestion hardening for gamepad, HOTAS/throttle/rudder, keyboard, and mouse is implemented.

Verified by:
- `components/charm_core/src/hid_semantic_model.cpp`
- `components/charm_core/src/decode_plan.cpp`
- `components/charm_core/src/hid_decoder.cpp`
- `tests/unit/test_hid_semantic_model.cpp`
- `tests/unit/test_decode_plan.cpp`
- `tests/unit/test_hid_decoder.cpp`
- `tests/unit/test_runtime_data_plane.cpp`
- `HID_INGESTION_MATRIX.md`

### Config compiler and runtime-effective apply path

Done:
- Browser draft -> `mapping_document` -> firmware compile -> persisted compiled bundle -> runtime activation is implemented.
- Serial-first `@CFG:` contract is implemented as `v2`.
- Mapping transforms are unit-tested for scale, inversion, deadzone, clamp, and runtime activation effects.

Verified by:
- `components/charm_core/src/config_compiler.cpp`
- `components/charm_app/src/config_transport_service.cpp`
- `components/charm_app/src/config_transport_runtime_adapter.cpp`
- `components/charm_app/src/config_activation.cpp`
- `components/charm_app/src/runtime_data_plane.cpp`
- `tests/unit/test_config_compiler.cpp`
- `tests/unit/test_mapping_engine.cpp`
- `tests/unit/test_config_transport_service.cpp`
- `tests/unit/test_config_transport_runtime_adapter.cpp`
- `tests/unit/test_config_activation.cpp`
- `tests/unit/test_runtime_data_plane.cpp`
- `charm-web-companion/__tests__/config.test.ts`
- `CONFIG_COMPILER_REPORT.md`
- `MAPPING_CORRECTNESS_MATRIX.md`

### Dual-profile firmware core path

Done:
- Profile manager supports two declared profiles.
- Generic and Xbox encoders both exist and are covered by automated tests.
- Runtime selection can encode through either profile, and unsupported profile selections fail closed.

Verified by:
- `components/charm_core/include/charm/core/profile_manager.hpp`
- `components/charm_core/src/profile_manager.cpp`
- `components/charm_core/src/profile_generic_gamepad_encoder.cpp`
- `components/charm_core/src/profile_wireless_xbox_controller_encoder.cpp`
- `components/charm_app/src/runtime_data_plane.cpp`
- `tests/unit/test_profile_manager.cpp`
- `tests/unit/test_profile_generic_gamepad_encoder.cpp`
- `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp`
- `tests/unit/test_runtime_data_plane.cpp`
- `tests/unit/test_config_transport_service.cpp`
- `PROFILE_OUTPUT_MATRIX.md`

### Current web companion baseline

Done:
- Full local Vitest suite passes.
- Production static build passes under Node 20.
- Current Config UI remains honest about the currently exposed profile being `profile_id = 1`.

Verified by:
- `charm-web-companion/__tests__/flash.test.ts`
- `charm-web-companion/__tests__/console.test.ts`
- `charm-web-companion/__tests__/config.test.ts`
- `charm-web-companion/__tests__/rendering.test.tsx`
- `charm-web-companion/__tests__/environment.test.tsx`
- `charm-web-companion/components/views/ConfigView.tsx`
- `charm-web-companion/lib/schema.ts`

## Done But Unverified On Hardware

These surfaces have implementation and local automated proof, but the required retained physical evidence is still missing.

| Area | Current implemented state | Missing hardware proof |
| --- | --- | --- |
| USB ingestion on real hardware | Firmware ingestion/decode/runtime path is implemented and unit-covered | `USB-01`, `USB-02` evidence in `evidence/<date>/<scenario>/` |
| Compiler-backed config/apply | Serial-first `v2` config compiler/apply path is implemented and locally verified | `CFG-01`, `CFG-03`, reboot/reconnect evidence |
| Generic BLE profile | Generic profile encode path exists and BLE adapter exists | `BLE-01`, `BLE-03` retained evidence |
| Flash -> reboot -> console | Web flash/console paths and firmware serial fixes exist | `OPS-01` retained evidence |
| Startup persisted activation | Config activation path is implemented and unit-tested | `STG-01` retained evidence |
| Release integrity rehearsal | Release workflow and integrity script exist | `REL-01`, `REL-02` retained evidence |

## Still Missing

These are the net-new gaps still blocking full integration closure. Completed compiler, ingestion, and profile-core work should not be redone.

### 1. Wireless Xbox is not yet a real BLE transport contract

Why this is still missing:
- The firmware core can encode Xbox-shaped reports, but the BLE adapter still exposes a single fixed Generic HID identity and report map.
- `components/charm_platform_ble/src/ble_transport_adapter.cpp` is still hard-coded to:
  - device name `Charm Gamepad`
  - appearance `Gamepad`
  - one fixed report map with report ID `1`

Exact file-level tasks:
- `components/charm_ports/include/charm/ports/ble_transport_port.hpp`
  - Add the smallest honest profile-selection/configuration seam needed by the BLE transport.
- `components/charm_platform_ble/include/charm/platform/ble_transport_adapter.hpp`
  - Carry the active output-profile contract into the adapter/backend state.
- `components/charm_platform_ble/src/ble_transport_adapter.cpp`
  - Implement profile-aware report-map/device-identity selection.
  - Ensure Generic and Xbox report contracts are not mixed.
- `components/charm_app/src/runtime_data_plane.cpp`
  - Ensure the BLE transport is told which profile contract is active before notify.
- `tests/unit/test_ble_transport_adapter.cpp`
  - Add profile-sensitive advertising/report-map/notify assertions.
- `tests/unit/test_runtime_data_plane.cpp`
  - Add end-to-end assertions that active profile selection reaches BLE transport configuration.

Acceptance criteria:
- Selecting profile `1` configures the current Generic BLE contract.
- Selecting profile `2` configures a distinct Xbox-compatible BLE contract.
- BLE notify never emits an Xbox-shaped report while advertising the Generic report map.
- Automated tests cover both profile contracts at the BLE adapter boundary.

### 2. Web companion does not yet expose dual-profile selection

Why this is still missing:
- The current web Config UI is intentionally locked to `profile_id = 1`.
- `charm-web-companion/lib/configCompiler.ts`, `charm-web-companion/lib/schema.ts`, and `ConfigView.tsx` still hard-code the Generic-only selection path.

Exact file-level tasks:
- `charm-web-companion/lib/configCompiler.ts`
  - Replace the single hard-coded profile export with an honest supported-profile model.
- `charm-web-companion/lib/schema.ts`
  - Update the config payload schema so it validates the actual supported profile set, not only literal `1`.
- `charm-web-companion/components/views/ConfigView.tsx`
  - Add an honest profile-selection UX only after the BLE/firmware contract is actually available.
  - Preserve explicit messaging if hardware proof is still pending.
- `charm-web-companion/__tests__/config.test.ts`
  - Add payload and UI tests for both supported profile IDs.
- `charm-web-companion/__tests__/rendering.test.tsx`
  - Assert that the copy remains honest when profile selection is exposed.

Acceptance criteria:
- The browser can select `Generic BLE Gamepad` and `Wireless Xbox Controller` without using unsupported IDs.
- Persist payloads send the selected supported `profile_id`.
- UI copy does not imply broader firmware capability than the branch actually supports.

### 3. PR CI still does not enforce the web companion lane

Why this is still missing:
- Local web tests and build pass, but `.github/workflows/ci.yml` still runs firmware host tests and firmware build only.

Exact file-level tasks:
- `.github/workflows/ci.yml`
  - Add Node 20 setup, dependency install, `npx vitest run`, and `npm run build` for `charm-web-companion`.
- `.github/workflows/release.yml`
  - Decide whether release preflight should also enforce the same web checks or explicitly depend on CI for them.
- `.github/workflows/deploy-webapp.yml`
  - Keep the deploy build commands aligned with CI so there is one source of truth.

Acceptance criteria:
- Pull requests fail if the web test suite fails.
- Pull requests fail if the static web build fails.
- CI uses the same Node/versioned install assumptions as local proof.

### 4. Status and roadmap docs have drifted behind the implemented code

Why this is still missing:
- Several docs still describe the compiler and second profile as merely planned.
- Examples observed during validation:
  - `README.md`
  - `charm-web-companion/README.md`
  - `ARCHITECTURE.md`
  - `charm-web-companion/ARCHITECTURE.md`
  - `CURRENT_TASK.md`
  - `TODO.md`
  - `TEST_STRATEGY.md`
  - `FEATURE_MATRIX.md`
  - `HARDWARE_VALIDATION_PACK.md` (still references current `v1` config wording)

Exact file-level tasks:
- `README.md`
- `charm-web-companion/README.md`
- `ARCHITECTURE.md`
- `charm-web-companion/ARCHITECTURE.md`
- `CURRENT_TASK.md`
- `TODO.md`
- `FEATURE_MATRIX.md`
- `TEST_STRATEGY.md`
- `HARDWARE_VALIDATION_PACK.md`

Acceptance criteria:
- No status doc says the compiler or Xbox firmware profile is still only planned.
- Config contract docs consistently describe the implemented serial-first `v2` path.
- Remaining work is described as BLE integration, web exposure, CI enforcement, and hardware evidence only.

### 5. Mandatory retained hardware evidence is still absent

Why this is still missing:
- `evidence/` does not exist yet.
- The release gate requires retained scenario artifacts, not ad hoc validation memory.

Exact file-level tasks:
- `HARDWARE_VALIDATION_PACK.md`
  - Refresh any stale scenario wording before execution.
- `VALIDATION.md`
  - Keep automated-vs-hardware boundaries synchronized with actual executed evidence.
- `evidence/<YYYYMMDD>/<scenario-id>/commands.txt`
- `evidence/<YYYYMMDD>/<scenario-id>/serial.log`
- `evidence/<YYYYMMDD>/<scenario-id>/result.json`

Acceptance criteria:
- Required scenarios have retained evidence for:
  - `CFG-01`
  - `CFG-03`
  - `USB-01`
  - `USB-02`
  - `BLE-01`
  - `BLE-02`
  - `BLE-03`
  - `OPS-01`
  - `STG-01`
  - `REL-01`
  - `REL-02`
- Release docs can point to concrete retained evidence paths, not planned scenario IDs.

## Net-New Work Only

Do not reopen these completed implementation lanes unless a regression is found:
- USB ingestion hardening
- config compiler core implementation
- mapping correctness/runtime-effective apply
- dual-profile manager and encoder core
- current local firmware unit suite and local web test/build baseline

The remaining work is integration closure:
- BLE profile contract
- web dual-profile exposure
- CI enforcement
- status/doc truth refresh
- retained hardware evidence
