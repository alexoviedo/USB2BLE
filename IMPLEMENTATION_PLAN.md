# Implementation Plan

This document freezes the amended contract boundaries and defines the implementation order before any new production coding starts.

## Amendment Summary

This revision supersedes the earlier narrow freeze in two material ways:

- The target output set is now exactly two profiles:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- The config compiler is now required production work, not deferred cleanup.

The following boundaries remain fixed:

- Serial-first `@CFG:` stays the configuration transport carrier unless implementation proves a strict need for a versioned expansion under the same framing.
- The webapp must remain honest about firmware capabilities at every step.

## Planning Intent

- Freeze the target product outcome before implementation so compiler, profile, transport, and UX work do not drift apart.
- Preserve the shortest honest contract path instead of widening the product surface casually.
- Treat hardware evidence and release gating as part of implementation completion, not optional follow-up.

## Frozen Contract Boundaries

These boundaries should remain fixed unless explicitly re-approved in a separate planning pass.

### 1. USB ingestion remains the physical source of truth

- USB host lifecycle, descriptor parsing, decode planning, and report ingestion remain the single upstream source for runtime input state.
- Compiler and profile work build on top of that source. They do not replace it with simulated or UI-authored input semantics.

### 2. Config compiler is in scope and becomes the mapping truth boundary

- `MappingConfigDocument -> CompiledMappingBundle` is required production behavior.
- `ValidateConfig` and `CompileConfig` must be implemented as real production code, not test doubles or stubbed pass-through logic.
- Compiler output must carry stable bundle metadata and deterministic diagnostics suitable for automated tests and operator-facing documentation.

### 3. Output profile scope is fixed to two product profiles

- The approved profile set for this implementation lane is:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- No third output profile should land in this lane.
- A profile is not considered “implemented” unless the encoder path, selection path, BLE/report compatibility, docs, and proof all exist.

### 4. Serial-first `@CFG:` remains the config carrier

- Wire framing remains `@CFG:{json}\n`.
- Serial remains the required transport carrier for config operations in this lane.
- BLE configuration transport remains out of scope.
- We begin from the existing v1 `persist`/`load`/`clear`/`get_capabilities` contract.
- If compiler-backed apply cannot be represented safely by the current command surface, the only allowed expansion is a versioned serial-first contract update documented before implementation lands in that slice.

### 5. Honest device/apply boundary

- Bringing the compiler into scope does not automatically mean raw draft JSON persistence on-device.
- The device may accept or persist compiled config material only where firmware truly supports it.
- The UI must not imply that the firmware stores arbitrary rich drafts unless that capability is actually implemented and documented.

### 6. Honest UI boundary

- `Config` may expose draft authoring, validation, compile, and device apply only where the underlying compiler and firmware support them.
- `Validate` remains a browser/Gamepad surface unless a separate implementation adds real firmware/runtime inspection.
- `Flash` and `Console` continue to reflect real port ownership and device behavior only.

### 7. Serial ownership stays mutually exclusive

- Owners remain `none | flash | console | config`.
- Flash, console, and config operations must continue to serialize access.

### 8. Release artifact boundary stays simple

- Release artifact set remains:
  - `bootloader.bin`
  - `partition-table.bin`
  - `charm.bin`
  - integrity/provenance metadata
- No OTA/update orchestration or backend service work is included in this lane.

## Dependency Order

The implementation order follows the required product chain:

1. USB HID ingestion
2. Mapping/compiler pipeline
3. Profile outputs
4. Config/apply over serial-first `@CFG`
5. Web UX alignment
6. Hardware gates
7. Release closure

Cross-cutting prerequisite work:

- Source-of-truth document alignment
- CI baseline closure
- Contract documentation updates whenever transport or capability boundaries change

## Milestone Gates

| Milestone | Goal | Depends on | Gate to exit |
|---|---|---|---|
| M0 | Plan approval and amended contract freeze | none | These planning docs are approved with no unresolved scope conflicts. |
| M1 | Source-of-truth alignment and CI baseline | M0 | Docs, workflows, and current code all describe the same amended target; firmware tests/builds and webapp tests/builds are reproducible locally and ready for CI enforcement. |
| M2 | USB ingestion + compiler foundation | M1 | USB ingestion invariants remain green, and a deterministic compiler path with diagnostics exists in automated tests. |
| M3 | Dual-profile output lane | M2 | Generic BLE Gamepad and Wireless Xbox Controller are both selectable, encodable, documented, and covered by automated tests. |
| M4 | Serial config/apply integration | M3 | The approved serial-first contract can validate/apply compiled config semantics honestly, with firmware and web adapter proof. |
| M5 | Web UX production hardening | M4 | Flash/console/config/validate surfaces match real firmware/browser capabilities and are covered by web tests. |
| M6 | Hardware evidence and release closure | M5 | Mandatory hardware scenarios for compiler/apply and both profiles have retained evidence, and release/integrity gates are reproducible. |

## File-Level Implementation Plan

This is the expected future touch set after approval. It is grouped by milestone to keep changes targeted.

### M1: Source-of-Truth Alignment and CI Baseline

| Files | Planned work |
|---|---|
| `README.md` | Reconcile repo-level product description with the amended target: compiler-backed config plus two output profiles. |
| `ARCHITECTURE.md` | Align layer summary and runtime claims with compiler-in-scope and dual-profile scope. |
| `CURRENT_TASK.md` | Replace stale framing with the approved implementation lane and milestone order. |
| `TODO.md` | Convert backlog into milestone-aligned execution items tied to compiler/profile/hardware gates. |
| `VALIDATION.md` | Align automated vs hardware-required proof language with compiler and dual-profile scope. |
| `HARDWARE_VALIDATION_PACK.md` | Expand scenario instructions to cover compiler/apply and both target output profiles. |
| `CONFIG_TRANSPORT_CONTRACT.md` | Document whether current v1 is sufficient or where a versioned serial-first expansion becomes required. |
| `charm-web-companion/ARCHITECTURE.md` | Preserve honest UI boundaries while reflecting compiler-backed config/apply work. |
| `charm-web-companion/README.md` | Align local dev and operator language with the amended product target. |
| `charm-web-companion/HANDOFF.md` | Remove drift from current production intent and support expectations. |
| `.github/workflows/ci.yml` | Add explicit webapp validation lane and preserve firmware lanes. |
| `.github/workflows/deploy-webapp.yml` | Reuse the same validated build assumptions used in PR CI. |
| `.github/workflows/release.yml` | Keep release assumptions aligned with the amended build/test contract. |

### M2: USB Ingestion and Compiler Foundation

| Files | Planned work |
|---|---|
| `components/charm_platform_usb/src/usb_host_adapter.cpp` | Tighten lifecycle/report/error semantics only where compiler/profile integration exposes ambiguity. |
| `components/charm_app/src/runtime_data_plane.cpp` | Replace or isolate the current synthetic runtime-bundle story so authored compiled bundles become the real mapping truth. |
| `components/charm_core/include/charm/core/config_compiler.hpp` | Finalize compiler contract details, diagnostics expectations, and invariants. |
| `components/charm_core/src/config_compiler.cpp` | Add the production compiler implementation. |
| `components/charm_core/include/charm/core/mapping_bundle.hpp` | Tighten compiled-bundle semantics only as needed for a real compiler output contract. |
| `components/charm_core/src/mapping_bundle.cpp` | Preserve bundle validation/hash semantics as the compiler’s downstream contract. |
| `components/charm_core/src/mapping_engine.cpp` | Ensure compiled bundles remain directly consumable by the mapping engine. |
| `components/charm_core/CMakeLists.txt` | Add the compiler implementation to the core build. |
| `tests/unit/test_runtime_data_plane.cpp` | Add authored-bundle activation expectations and determinism assertions. |
| `tests/unit/test_config_compiler.cpp` | Add compiler validation/compile/diagnostics fixture coverage. |
| `tests/unit/CMakeLists.txt` | Register new compiler tests and any extra core sources required by the host suite. |

### M3: Dual-Profile Output Lane

| Files | Planned work |
|---|---|
| `components/charm_core/include/charm/core/profile_manager.hpp` | Define the two-profile contract and capability accessors. |
| `components/charm_core/src/profile_manager.cpp` | Expand selection/encoding/capability logic from one profile to two. |
| `components/charm_core/src/profile_generic_gamepad_encoder.cpp` | Promote the existing encoder into the fully documented Generic BLE Gamepad reference path. |
| `components/charm_core/src/profile_wireless_xbox_controller_encoder.cpp` | Add the new Wireless Xbox Controller encoder. |
| `components/charm_platform_ble/src/ble_transport_adapter.cpp` | Support any required per-profile descriptor, report-map, advertising, or notify differences. |
| `components/charm_contracts/include/charm/contracts/report_types.hpp` | Adjust report metadata only if profile-specific output needs it. |
| `tests/unit/test_profile_manager.cpp` | Add profile selection and capability coverage for both target profiles. |
| `tests/unit/test_profile_generic_gamepad_encoder.cpp` | Expand coverage to match the formal Generic BLE Gamepad product contract. |
| `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp` | Add the new encoder test suite. |
| `tests/unit/test_ble_transport_adapter.cpp` | Add assertions for profile-sensitive BLE/report behavior if required. |

### M4: Serial Config/Apply Integration

| Files | Planned work |
|---|---|
| `CONFIG_TRANSPORT_CONTRACT.md` | Lock the final serial-first config/apply contract after M2 and M3 expose real payload needs. |
| `components/charm_contracts/include/charm/contracts/config_transport_types.hpp` | Add or refine contract types only if required by the approved serial-first expansion. |
| `components/charm_app/src/config_transport_service.cpp` | Implement compiler-backed config validation/apply/persist semantics. |
| `components/charm_app/src/config_transport_runtime_adapter.cpp` | Implement parser/serializer changes only within the approved serial-first framing. |
| `components/charm_app/src/config_activation.cpp` | Ensure startup activation matches the real compiled-config + selected-profile path. |
| `components/charm_platform_storage/src/config_store_nvs.cpp` | Persist only the approved compiled-config/profile state needed by the final contract. |
| `tests/unit/test_config_transport_service.cpp` | Add success and rejection coverage for the amended apply path. |
| `tests/unit/test_config_transport_runtime_adapter.cpp` | Add framing, payload-shape, versioning, and malformed-request coverage for the final contract. |
| `tests/unit/test_config_activation.cpp` | Add activation assertions for compiler-backed state. |

### M5: Web UX Production Hardening

| Files | Planned work |
|---|---|
| `charm-web-companion/components/views/FlashView.tsx` | Keep flash UX stable and accurate as the gateway into config/profile validation. |
| `charm-web-companion/components/views/ConsoleView.tsx` | Preserve explicit ownership and runtime-port truth. |
| `charm-web-companion/components/views/ConfigView.tsx` | Add honest compile/validate/apply UX against the real serial-first firmware contract. |
| `charm-web-companion/components/views/ValidateView.tsx` | Keep browser-only validation scope unmistakable even if profile visibility expands. |
| `charm-web-companion/lib/adapters/SerialConfigTransport.ts` | Implement the approved serial-first config/apply client contract. |
| `charm-web-companion/lib/schema.ts` | Expand config schema only as needed for the compiler-backed authoring/apply flow. |
| `charm-web-companion/lib/adapters/Flasher.ts` | Keep flash behavior aligned with the same release artifact boundary. |
| `charm-web-companion/lib/adapters/SerialMonitor.ts` | Preserve console ownership and reconnect behavior while config flow grows. |
| `charm-web-companion/__tests__/config.test.ts` | Add compile/apply and truth-boundary coverage. |
| `charm-web-companion/__tests__/console.test.ts` | Keep serial ownership/reuse behavior covered as config and console share the port. |
| `charm-web-companion/__tests__/flash.test.ts` | Keep flash behavior covered as broader workflow integration grows. |
| `charm-web-companion/__tests__/rendering.test.tsx` | Assert honest UI copy for compiler/apply/profile scope. |
| `charm-web-companion/docs/serial-lifecycle-state-machine.md` | Keep ownership documentation synchronized with real app behavior. |

### M6: Hardware Evidence and Release Closure

| Files | Planned work |
|---|---|
| `HARDWARE_VALIDATION_PACK.md` | Use as the canonical operator pack for both target profiles and compiler/apply scenarios. |
| `VALIDATION.md` | Record what becomes CI-proved vs hardware-proved after closure work. |
| `scripts/generate_release_integrity.sh` | Keep release integrity output stable and machine-verifiable. |
| `README.md` and `charm-web-companion/README.md` | Reflect final validated product claims once evidence exists. |

## Explicitly Deferred Work

These items remain out of scope for this lane unless you explicitly expand scope again later.

- BLE configuration transport
- Raw arbitrary JSON draft persistence on the device
- A third or fourth output profile beyond the two approved profiles
- OTA/update orchestration or backend services

## Plan Risk Notes

1. The biggest architecture risk is implementing the compiler without freezing how compiled config semantics reach the runtime and storage boundaries.
2. The biggest scope-creep risk is smuggling raw-draft persistence into the product under the banner of “compiler support.”
3. The biggest proof risk is shipping one profile solidly and the second profile only nominally, without equivalent automated and hardware evidence.
