# FINAL_DELTA_EXECUTION_PLAN.md

## Scope Guardrail

This execution plan is derived from:
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`

It covers only the remaining integration-closure work.

The following completed implementation lanes must not be reopened except for narrow integration fixes:
- USB ingestion hardening
- config compiler core implementation
- mapping correctness/runtime-effective apply
- dual-profile manager and encoder core
- current local firmware unit suite baseline
- current local web Vitest/build baseline

## Numbered Execution Backlog

### 1. Complete the BLE-facing Xbox profile surface and proof

Goal:
- Turn the existing Xbox firmware encoder into a real BLE-facing profile contract instead of a core-only encode path.

Exact files to touch:
- `components/charm_ports/include/charm/ports/ble_transport_port.hpp`
- `components/charm_platform_ble/include/charm/platform/ble_transport_adapter.hpp`
- `components/charm_platform_ble/src/ble_transport_adapter.cpp`
- `components/charm_app/src/runtime_data_plane.cpp`
- `tests/unit/test_ble_transport_adapter.cpp`
- `tests/unit/test_runtime_data_plane.cpp`
- `PROFILE_OUTPUT_MATRIX.md`
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`

Acceptance criteria:
- Active profile `1` configures the current Generic BLE-facing contract.
- Active profile `2` configures a distinct Xbox-compatible BLE-facing contract.
- BLE advertising/report-map/device identity are profile-aware and deterministic.
- BLE notify never emits an Xbox report while the adapter is configured for the Generic contract.
- Runtime profile selection reaches the BLE transport boundary explicitly, not by accidental report-shape drift.
- Automated tests prove both profile contracts at the BLE adapter boundary.

Tests/evidence required:
- `cmake --build build/unit --parallel`
- `ctest --test-dir build/unit --output-on-failure`
- Focused proof:
  - `ctest --test-dir build/unit --output-on-failure -R 'BleTransportAdapterTest|RuntimeDataPlaneTest|ProfileManagerTest|ProfileWirelessXboxControllerEncoderTest'`
- Updated `PROFILE_OUTPUT_MATRIX.md` with BLE-facing contract details
- Later hardware evidence dependency:
  - `BLE-01`
  - `BLE-02`
  - `BLE-03`

Rollback plan:
- Keep the current Generic BLE contract as the rollback-safe baseline.
- If Xbox BLE integration destabilizes advertising/report behavior, revert only the BLE transport/profile-plumbing changes in:
  - `components/charm_ports/include/charm/ports/ble_transport_port.hpp`
  - `components/charm_platform_ble/include/charm/platform/ble_transport_adapter.hpp`
  - `components/charm_platform_ble/src/ble_transport_adapter.cpp`
  - `components/charm_app/src/runtime_data_plane.cpp`
- Preserve the already-complete core Xbox encoder and manager support unless a regression proves they are implicated.

### 2. Add web Config UI support for profile `1` and profile `2`

Goal:
- Expose only the truly supported profile set in the browser once the BLE-facing contract is real.

Exact files to touch:
- `charm-web-companion/lib/configCompiler.ts`
- `charm-web-companion/lib/schema.ts`
- `charm-web-companion/components/views/ConfigView.tsx`
- `charm-web-companion/__tests__/config.test.ts`
- `charm-web-companion/__tests__/rendering.test.tsx`
- `CONFIG_TRANSPORT_CONTRACT.md`
- `CONFIG_COMPILER_REPORT.md`
- `PROFILE_OUTPUT_MATRIX.md`
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`

Acceptance criteria:
- The Config UI presents exactly two supported profiles:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- Persist payloads send `profile_id = 1` or `profile_id = 2` only.
- Schema validation rejects unsupported profile IDs.
- UI copy stays honest about firmware behavior and any remaining hardware-proof limits.
- No UI implies extra profiles, raw-draft device persistence, or unsupported firmware capabilities.

Tests/evidence required:
- `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
- `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`
- Focused proof:
  - `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run __tests__/config.test.ts __tests__/rendering.test.tsx`
- Updated docs confirming the exposed profile set
- Later hardware evidence dependency:
  - `CFG-03`
  - `BLE-01`
  - `BLE-02`

Rollback plan:
- Revert only the web profile-selection exposure in:
  - `charm-web-companion/lib/configCompiler.ts`
  - `charm-web-companion/lib/schema.ts`
  - `charm-web-companion/components/views/ConfigView.tsx`
  - related web tests/docs
- Fall back to the current honest Generic-only UI if dual-profile exposure creates truth-boundary drift.

### 3. Enforce the web lane in PR CI

Goal:
- Make PR automation enforce the same web validation already passing locally.

Exact files to touch:
- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- `.github/workflows/deploy-webapp.yml`
- `README.md`
- `VALIDATION.md`
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`

Acceptance criteria:
- PR CI installs web dependencies under Node 20.
- PR CI runs the web Vitest suite.
- PR CI runs the web production build.
- Release and deploy workflows do not drift from the same Node/install/build assumptions.
- Workflow docs/status reflect that the web lane is no longer local-only proof.

Tests/evidence required:
- Workflow YAML review against local commands
- Local dry-run parity check by executing:
  - `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
  - `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`
- Post-change CI proof on the eventual PR:
  - passing `ci.yml` web test step
  - passing `ci.yml` web build step

Rollback plan:
- If workflow churn blocks unrelated PRs, revert only the workflow-file changes while preserving local code/test changes.
- Keep deploy and release jobs working even if CI web enforcement must be temporarily simplified.

### 4. Clean up stale docs and status surfaces

Goal:
- Bring status/roadmap docs up to the current implemented branch truth so no completed milestone reads as still planned.

Exact files to touch:
- `README.md`
- `ARCHITECTURE.md`
- `CURRENT_TASK.md`
- `TODO.md`
- `VALIDATION.md`
- `HARDWARE_VALIDATION_PACK.md`
- `FEATURE_MATRIX.md`
- `TEST_STRATEGY.md`
- `charm-web-companion/README.md`
- `charm-web-companion/ARCHITECTURE.md`
- `charm-web-companion/HANDOFF.md`
- `DOC_ALIGNMENT_REPORT.md`
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`
- `FINAL_DELTA_EXECUTION_PLAN.md`

Acceptance criteria:
- No doc says the compiler is still only planned.
- No doc says the Xbox firmware profile is still only planned at the core/runtime layer.
- Docs clearly distinguish:
  - completed firmware/core work
  - remaining BLE-facing integration
  - remaining web exposure work
  - remaining CI/hardware-gate work
- Config docs consistently describe the implemented serial-first `v2` contract.
- Hardware docs consistently describe retained evidence as a hard release gate.

Tests/evidence required:
- Manual doc consistency pass across the files above
- Spot-check searches for stale language such as:
  - “planned”
  - “one concrete profile encoder path”
  - “v1”
  - “compiler not yet implemented”
- Updated `DOC_ALIGNMENT_REPORT.md` summarizing each doc touch and why

Rollback plan:
- Revert only the doc/status files if wording changes introduce inaccuracies.
- Do not tie doc cleanup to code changes unless a code/document mismatch must be corrected together.

### 5. Close the retained hardware evidence gate

Goal:
- Convert the remaining unverified hardware scenarios into retained release evidence.

Exact files to touch:
- `HARDWARE_VALIDATION_PACK.md`
- `VALIDATION.md`
- `README.md`
- `charm-web-companion/README.md`
- `INTEGRATION_DELTA.md`
- `UPDATED_MILESTONE_STATUS.md`
- `evidence/<YYYYMMDD>/<scenario-id>/commands.txt`
- `evidence/<YYYYMMDD>/<scenario-id>/serial.log`
- `evidence/<YYYYMMDD>/<scenario-id>/result.json`

Acceptance criteria:
- `evidence/` exists and contains retained artifacts for all mandatory scenarios:
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
- Each scenario includes:
  - `commands.txt`
  - `serial.log`
  - `result.json`
- Release/status docs point to concrete retained evidence paths, not hypothetical scenario IDs.

Tests/evidence required:
- Hardware execution per `HARDWARE_VALIDATION_PACK.md`
- Release integrity check:
  - `sha256sum -c SHA256SUMS`
  - provenance inspection
- Physical validation evidence for:
  - flash -> reboot -> console
  - config get/persist/load/clear behavior
  - USB enumeration on target hardware
  - both Generic and Xbox profile runtime behavior
  - disconnect/reconnect recovery
  - rollback rehearsal

Rollback plan:
- Evidence collection itself should not require code rollback.
- If a hardware scenario fails because of a code regression, stop evidence collection, open a new focused implementation task on the failing lane, and leave existing evidence artifacts intact as failure evidence.

## Recommended Execution Order

1. Complete BLE-facing Xbox profile surface and proof.
2. Expose web Config profile support for `1` and `2`.
3. Enforce the web lane in PR CI.
4. Refresh stale docs/status surfaces to the current truth.
5. Execute retained hardware evidence closure.

## Approval Gate

This plan intentionally stops at execution planning.

No implementation work should begin until this file is approved.
