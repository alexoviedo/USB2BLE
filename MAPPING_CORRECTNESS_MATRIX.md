# MAPPING_CORRECTNESS_MATRIX.md

| Area | Status | Proof |
| --- | --- | --- |
| Mapping document validation | Implemented | `test_config_compiler.cpp` rejects empty, malformed, bad clamp, unknown target, and duplicate-source configs |
| Deterministic compile output | Implemented | `test_config_compiler.cpp` verifies repeated compile output is byte-stable |
| Global scale application | Implemented | `test_config_compiler.cpp` checks combined compiled scale values |
| Per-axis scale application | Implemented | `test_config_compiler.cpp`, `test_mapping_engine.cpp` |
| Inversion | Implemented | `test_config_compiler.cpp` verifies negative compiled scale from `invert=true` |
| Deadzone | Implemented | `test_config_compiler.cpp` verifies compiled deadzone; `test_mapping_engine.cpp` verifies runtime deadzone effect |
| Clamp min/max | Implemented | `test_config_compiler.cpp` verifies compiled clamp bounds; `test_mapping_engine.cpp` verifies runtime clamp effect |
| Button mapping | Implemented | `test_config_compiler.cpp` and `test_mapping_engine.cpp` |
| Persist flow stores compiled runtime bundle | Implemented | `test_config_transport_service.cpp` verifies compiled bundle bytes are persisted |
| Startup/load activation | Implemented | `test_config_activation.cpp` verifies persisted compiled bundle becomes the active runtime bundle |
| Clear flow drops active compiled bundle | Implemented | `test_config_transport_service.cpp`, `test_mapping_bundle.cpp` |
| Runtime activation changes encoded output | Implemented | `test_runtime_data_plane.cpp` proves active compiled mappings redirect output away from default axis assignment |
| Safe fallback for unmapped inputs | Implemented | `RuntimeDataPlane` fallback path exercised by `test_runtime_data_plane.cpp` and existing default-path tests |
| Web payload is compiler-backed, not manual bundle-ref spoofing | Implemented | `charm-web-companion/__tests__/config.test.ts` verifies `mapping_document` wire format |
| Raw draft persistence on device | Not supported by design | Contract docs and UI copy remain explicit about compiled-bundle-only persistence |
