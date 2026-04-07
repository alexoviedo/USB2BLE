# Feature Matrix

This matrix reflects the current implemented state on `codex/freeze-contract-plan`.

Status legend:
- `implemented`: code/docs/workflow support exists and is validated automatically
- `partial`: implemented, but one explicitly documented non-release gap remains
- `unverified`: implemented, but retained hardware evidence is still required before release claims
- `out_of_scope`: intentionally not supported by the current product contract

| Area | Capability | Current Status | Evidence | Remaining Limit |
| --- | --- | --- | --- | --- |
| USB HID ingestion | Descriptor parse -> decode plan -> HID decode -> runtime ingestion for gamepad, HOTAS/pedals, keyboard, and mouse | implemented | `components/charm_core/src/hid_semantic_model.cpp`, `components/charm_core/src/decode_plan.cpp`, `components/charm_core/src/hid_decoder.cpp`, `components/charm_app/src/runtime_data_plane.cpp`, related unit tests, `HID_INGESTION_MATRIX.md` | Real multi-device/hub evidence is still a hardware gate |
| Mapping engine | Production config compiler (`MappingConfigDocument` -> `CompiledMappingBundle`) | implemented | `components/charm_core/src/config_compiler.cpp`, `tests/unit/test_config_compiler.cpp`, `CONFIG_COMPILER_REPORT.md`, `MAPPING_CORRECTNESS_MATRIX.md` | Hardware proof of compile/apply/reboot/reconnect still required |
| Mapping engine | Runtime-effective apply of persisted compiled config | implemented | `components/charm_app/src/config_transport_service.cpp`, `components/charm_app/src/config_activation.cpp`, `components/charm_app/src/runtime_data_plane.cpp`, related unit tests | Hardware proof still required |
| Profile outputs | `profile_id = 1` `Generic BLE Gamepad` | implemented | `components/charm_core/src/profile_generic_gamepad_encoder.cpp`, `components/charm_platform_ble/src/ble_transport_adapter.cpp`, `tests/unit/test_profile_generic_gamepad_encoder.cpp`, `tests/unit/test_ble_transport_adapter.cpp`, `PROFILE_OUTPUT_MATRIX.md` | BLE pairing/runtime evidence still required |
| Profile outputs | `profile_id = 2` `Wireless Xbox Controller` | implemented | `components/charm_core/src/profile_wireless_xbox_controller_encoder.cpp`, `components/charm_platform_ble/src/ble_transport_adapter.cpp`, `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp`, `tests/unit/test_ble_transport_adapter.cpp`, `PROFILE_OUTPUT_MATRIX.md` | BLE pairing/runtime evidence still required |
| Profile outputs | BLE-facing profile selection and contract carry-through | implemented | `components/charm_ports/include/charm/ports/ble_transport_port.hpp`, `components/charm_platform_ble/src/ble_transport_adapter.cpp`, `components/charm_app/src/runtime_data_plane.cpp`, related unit tests | Real host/peer BLE behavior still requires retained evidence |
| Config transport/apply | Serial-first `@CFG:` `v2` transport with `persist`, `load`, `clear`, `get_capabilities` | implemented | `CONFIG_TRANSPORT_CONTRACT.md`, `components/charm_app/src/config_transport_runtime_adapter.cpp`, `tests/unit/test_config_transport_runtime_adapter.cpp`, `tests/unit/test_config_transport_service.cpp` | `get_capabilities` does not enumerate profile IDs; UI must keep that boundary honest |
| Config transport/apply | Raw JSON draft persistence on-device | out_of_scope | `CONFIG_TRANSPORT_CONTRACT.md`, `charm-web-companion/ARCHITECTURE.md` | Intentionally unsupported |
| Web companion | Flash and serial console workflows | implemented | `charm-web-companion/components/views/FlashView.tsx`, `charm-web-companion/components/views/ConsoleView.tsx`, `charm-web-companion/__tests__/flash.test.ts`, `charm-web-companion/__tests__/console.test.ts` | Real board evidence still required |
| Web companion | Config UI with constrained profile selection for `1` and `2` | implemented | `charm-web-companion/components/views/ConfigView.tsx`, `charm-web-companion/lib/configProfiles.ts`, `charm-web-companion/__tests__/config.test.ts`, `charm-web-companion/__tests__/config-view.test.tsx`, `WEB_PROFILE_UI_MATRIX.md` | Still serial-first only; no BLE config transport |
| Web companion | Validate view truth boundary | implemented | `charm-web-companion/components/views/ValidateView.tsx`, `charm-web-companion/ARCHITECTURE.md`, rendering tests | Browser-side only; not a firmware/BLE inspector |
| CI / release | Firmware host CTest in CI | implemented | `.github/workflows/ci.yml`, `.github/workflows/release.yml` | None |
| CI / release | ESP-IDF firmware build in CI | implemented | `.github/workflows/ci.yml`, `.github/workflows/release.yml` | None |
| CI / release | Web Vitest + production build enforced on PRs | implemented | `.github/workflows/ci.yml`, `.github/workflows/deploy-webapp.yml`, `CI_WEB_ENFORCEMENT_REPORT.md` | Web lint is deferred until `2026-04-20` because the current `@rushstack/eslint-patch` path is unstable with ESLint 9 |
| CI / release | Retained mandatory hardware evidence and release closure | unverified | `VALIDATION.md`, `HARDWARE_VALIDATION_PACK.md` | This is the primary remaining release blocker |

## Matrix Takeaways

1. The compiler, dual-profile firmware lane, dual-profile web Config UX, and PR CI web enforcement are implemented on this branch.
2. The main remaining release gap is retained hardware evidence, not missing core implementation.
3. The only intentionally deferred automated check is web lint, with an explicit target date of `2026-04-20`.
