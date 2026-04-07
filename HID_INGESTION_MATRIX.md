# HID Ingestion Matrix

This matrix reflects the current USB HID ingestion state on branch `codex/freeze-contract-plan` after the target-class ingestion hardening pass.

## Common Path Status

| Area | Status | Notes |
| --- | --- | --- |
| HID semantic parse | Implemented | Input-only field extraction now preserves logical ranges, relative flags, array metadata, null-state metadata, and usage ranges. |
| Decode plan build | Implemented | Device identity, class-aware element typing, and field validation are now deterministic. |
| Report decode | Implemented | Axes, triggers, hats, buttons, and keyboard arrays normalize into stable `InputElementEvent` output. |
| Runtime bundle synthesis | Implemented | Dynamic array-backed sources are deduped before assignment and bundle build, preventing duplicate keyboard-slot expansion. |
| Error handling | Implemented | Rejects malformed descriptors/reports, zero-sized fields, out-of-bounds reads, bundle-capacity overflow, and stale array state on reconnect. |

## Per-Class Matrix

| Class | Status | Normalization / Decode Coverage | Error Handling / Edge Cases | Test Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| Gamepad | Implemented and test-covered | Generic Desktop axes normalize to signed axis range, hats preserve null-state semantics, buttons normalize to `0/1`. | Malformed reports, cross-byte reads, zero-sized fields, and report-id filtering covered. | [tests/unit/test_hid_semantic_model.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_semantic_model.cpp), [tests/unit/test_decode_plan.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_decode_plan.cpp), [tests/unit/test_hid_decoder.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_decoder.cpp), [tests/unit/test_runtime_data_plane.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_runtime_data_plane.cpp) | Current output remains the Generic Gamepad profile. |
| HOTAS / throttle / rudder pedals | Implemented and test-covered | Simulation Controls rudder/aileron classify as axes; throttle/accelerator/brake/clutch classify as triggers and normalize to byte trigger range. | Descriptor classification, runtime flow-through, and trigger saturation are covered. | [tests/unit/test_hid_semantic_model.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_semantic_model.cpp), [tests/unit/test_decode_plan.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_decode_plan.cpp), [tests/unit/test_hid_decoder.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_decoder.cpp), [tests/unit/test_runtime_data_plane.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_runtime_data_plane.cpp) | Unit-backed; real HOTAS hardware evidence is still a separate validation step. |
| Keyboard | Implemented and test-covered | Keyboard array reports emit deterministic press/release button events per usage; duplicate usages across slots are deduped; reconnect resets stale array state. | Out-of-range array values are ignored per-slot; duplicate slot expansion is removed from runtime source assignment/build; reconnect state reset covered. | [tests/unit/test_hid_semantic_model.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_semantic_model.cpp), [tests/unit/test_decode_plan.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_decode_plan.cpp), [tests/unit/test_hid_decoder.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_decoder.cpp), [tests/unit/test_runtime_data_plane.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_runtime_data_plane.cpp) | Runtime button assignment is intentionally capped to the currently encodable 16-button Generic Gamepad output until profile expansion lands. |
| Mouse | Implemented and test-covered | Relative X/Y/Wheel decode as axes with signed normalization; button bits normalize to `0/1`; idle zero reports clear motion cleanly. | Relative-path runtime flow-through and zero-reset behavior are covered. | [tests/unit/test_hid_semantic_model.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_hid_semantic_model.cpp), [tests/unit/test_decode_plan.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_decode_plan.cpp), [tests/unit/test_runtime_data_plane.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/tests/unit/test_runtime_data_plane.cpp) | Unit-backed; real mouse hardware evidence is still a separate validation step. |

## Evidence Summary

- Build/test command: `cmake --build build/unit --parallel`
- Full suite: `ctest --test-dir build/unit --output-on-failure`
- Result: `18/18` tests passed

## Remaining Honest Boundaries

- This pass completes ingestion/decode normalization and deterministic unit coverage for the target USB HID classes.
- It does not expand BLE output profiles yet; keyboard and mouse inputs still terminate in the currently shipped Generic Gamepad output contract.
- Real-device hardware evidence for HOTAS, keyboard, and mouse peripherals is not part of this unit-only ingestion pass and should be collected in the later hardware-gate milestone.
