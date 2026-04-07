# UPDATED_MILESTONE_STATUS.md

## Branch State Summary

Validated on `codex/freeze-contract-plan` using the current local branch state.

Verification commands:
- `cmake --build build/unit --parallel`
- `ctest --test-dir build/unit --output-on-failure`
- `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
- `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`
- `bash -lc 'source /Users/alex/esp/esp-idf/export.sh && idf.py build'`

Observed results:
- Firmware CTest: `20/20` passing
- Web Vitest: `6/6` files passing, `60` tests passing
- Web production build: passing
- ESP-IDF firmware build: passing locally on the current branch state

## Milestone Status

| Milestone | Status | Current assessment | Exit blockers, if any |
| --- | --- | --- | --- |
| M0 | complete | Planning freeze and contract boundaries exist and have already guided implementation. | None |
| M1 | complete | Status docs, contract docs, and PR CI baseline now reflect the implemented branch truth. | None |
| M2 | complete | USB ingestion hardening and the production config compiler are implemented and locally verified. | None |
| M3 | complete | Dual-profile manager, encoders, and BLE-facing contract wiring are implemented and covered by automated tests. | Hardware BLE evidence still belongs to M6, not M3 |
| M4 | complete | Serial-first `@CFG:` `v2` config/apply flow is implemented, locally tested, and documented. | None |
| M5 | complete | Web Config UX exposes profiles `1` and `2`, PR CI enforces web tests/build, and the web companion docs match the current contract. | Web lint remains intentionally deferred outside the enforced PR lane until `2026-04-20` |
| M6 | partial | Retained evidence now exists for environment readiness, transport binding, and postfix transport isolation. Current branch truth is mixed: shell/browser `config.get_capabilities` is proven on the current flashed image, but browser `persist/load/clear/load` is still failing and most mandatory hardware scenarios remain unexecuted. | Release remains blocked by failed `CFG-BROWSER-ROUNDTRIP` evidence and the still-missing USB/BLE/OPS/STG/REL retained scenarios |

## Completed Milestones And Slices Not To Reopen

These items are already implemented and locally verified on this branch:

- USB ingestion hardening:
  - `components/charm_core/src/hid_semantic_model.cpp`
  - `components/charm_core/src/decode_plan.cpp`
  - `components/charm_core/src/hid_decoder.cpp`
  - related unit tests
- Config compiler and runtime-effective apply:
  - `components/charm_core/src/config_compiler.cpp`
  - `components/charm_app/src/config_transport_service.cpp`
  - `components/charm_app/src/config_transport_runtime_adapter.cpp`
  - `components/charm_app/src/config_activation.cpp`
  - related unit tests and web config tests
- Dual-profile firmware output lane:
  - `components/charm_core/src/profile_manager.cpp`
  - `components/charm_core/src/profile_generic_gamepad_encoder.cpp`
  - `components/charm_core/src/profile_wireless_xbox_controller_encoder.cpp`
  - `components/charm_platform_ble/src/ble_transport_adapter.cpp`
  - related unit tests
- Current web baseline:
  - dual-profile Config UI
  - all checked-in Vitest suites passing
  - production static build passing
  - PR CI web-quality lane enforced

If any of those areas are touched again, it should be for narrow bug fixes only, not to restart the milestone.

## Remaining Milestone Deltas

### To close M6

- Resolve and rerun the browser config CRUD lane so retained evidence shows:
  - `config.persist` success
  - `config.load` success after persist
  - `config.clear` success
  - `config.load` contract behavior after clear
- Execute and retain evidence for the remaining mandatory scenarios:
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
- Store results under `evidence/<YYYYMMDD>/<scenario-id>/`

Acceptance:
- every mandatory scenario has retained artifacts and an outcome consistent with release criteria
- release docs can point to retained evidence paths, not just scenario definitions
- no doc claims the config CRUD hardware lane is complete while `browser-roundtrip-proof.md` still records failure

### Deferred maintenance item

- Stabilize web lint for CI by `2026-04-20`
  - current blocker: `@rushstack/eslint-patch` fails before repo lint rules execute under ESLint 9
  - source of truth: `CI_WEB_ENFORCEMENT_REPORT.md`

## Net-New Work Confirmation

Only release-closure work remains.

The branch does **not** need:
- another USB ingestion implementation pass
- another compiler implementation pass
- another profile-manager/encoder/BLE contract pass
- another web Config profile exposure pass
- another PR CI web-enforcement pass

The branch **does** need:
- config CRUD hardware failure analysis and rerun
- retained hardware evidence execution for the remaining mandatory scenarios
- lint stabilization follow-up by `2026-04-20`
