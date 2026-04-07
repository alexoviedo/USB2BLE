# CONFIG_COMPILER_REPORT.md

## Outcome
The config path is now runtime-effective instead of UI-local.

Implemented flow:
1. Browser config drafts compile into a deterministic `mapping_document`.
2. `config.persist` sends that document over serial `@CFG:` using protocol `v2`.
3. Firmware validates and compiles the document into a `CompiledMappingBundle`.
4. Firmware persists the compiled bundle bytes, derived bundle metadata, profile ID, and optional bonding material.
5. Firmware activates the compiled bundle immediately in the runtime loader so new mappings take effect without reboot-only behavior.

## Contract
- Transport framing stayed serial-first `@CFG:`.
- `protocol_version` is now `2`.
- `config.persist` request payload is:
  - `mapping_document`
  - `profile_id`
  - optional `bonding_material`
- `config.load` response payload remains honest and narrow:
  - `mapping_bundle`
  - `profile_id`
- The device does not persist the raw browser draft.

## Integrity Semantics
- Bundle integrity is computed from the compiled bundle contents with `ComputeMappingBundleHash`.
- `mapping_bundle.integrity` is the bundle hash.
- `mapping_bundle.bundle_id` is derived from that integrity value.
- Persist failures leave the previous persisted compiled bundle authoritative.

## Runtime Effect
- `RuntimeDataPlane` now checks the active compiled bundle first for canonical compiler-source mappings.
- If an authored compiled rule matches the incoming source, the authored transform is applied.
- If no authored rule matches, runtime falls back to the deterministic discovered-device mapping bundle.
- This prevents configured mappings from degenerating into a bundle-ref seed that never changes actual output behavior.

## Main Files
- Firmware contracts and compiler:
  - `components/charm_contracts/include/charm/contracts/config_transport_types.hpp`
  - `components/charm_contracts/include/charm/contracts/requests.hpp`
  - `components/charm_core/include/charm/core/config_compiler.hpp`
  - `components/charm_core/src/config_compiler.cpp`
  - `components/charm_core/include/charm/core/mapping_bundle.hpp`
  - `components/charm_core/src/mapping_bundle.cpp`
  - `components/charm_core/src/mapping_engine.cpp`
- Firmware activation/runtime/storage:
  - `components/charm_app/src/config_transport_service.cpp`
  - `components/charm_app/src/config_transport_runtime_adapter.cpp`
  - `components/charm_app/src/config_activation.cpp`
  - `components/charm_app/src/runtime_data_plane.cpp`
  - `components/charm_platform_storage/src/config_store_nvs.cpp`
- Web companion:
  - `charm-web-companion/lib/configCompiler.ts`
  - `charm-web-companion/lib/schema.ts`
  - `charm-web-companion/components/views/ConfigView.tsx`

## Validation
- Firmware host tests:
  - `cmake --build build/unit --parallel`
  - `ctest --test-dir build/unit --output-on-failure`
- Web companion:
  - `PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run __tests__/config.test.ts`
  - `PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`

## Remaining Honest Limits
- The firmware/runtime output layer now supports `profile_id = 1` (`Generic BLE Gamepad`) and `profile_id = 2` (`Wireless Xbox Controller`), but the current web companion Config UI still persists `profile_id = 1` only.
- `config.load` returns bundle/profile metadata, not the raw compiled bundle or full browser draft.
- The parser/compiler currently expects the documented `mapping_document` JSON shape and rejects anything outside it.
