# TODO.md

## Status legend
- queued
- in_progress
- done
- blocked

## Integrated Backlog

| Order | Item | Status | Notes |
| --- | --- | --- | --- |
| 1 | Align repo/web/contract docs and freeze release criteria | done | Current docs now reflect compiler-backed `v2`, dual-profile support, and hard hardware release gating |
| 2 | Close automated baseline for firmware + web companion | done | Firmware tests/builds plus web companion tests/builds are enforced in PR CI |
| 3 | Implement production config compiler | done | `MappingConfigDocument -> CompiledMappingBundle` exists with tests/docs |
| 4 | Formalize `Generic BLE Gamepad` profile support | done | BLE-facing contract, docs, and automated proof are in place |
| 5 | Add `Wireless Xbox Controller` profile support | done | Encoder, BLE-facing contract, web exposure, and automated proof are in place |
| 6 | Execute mandatory hardware evidence pack | in_progress | `config.get_capabilities` is now retained on hardware, but browser `persist/load/clear/load` still fails and the remaining mandatory scenarios are unexecuted |
| 7 | Stabilize web lint for CI | blocked | Deferred until `2026-04-20`; current `@rushstack/eslint-patch` path fails before repo lint rules execute |
| 8 | Cut release candidate with retained evidence and integrity verification | queued | Requires all mandatory evidence and successful checksum/provenance verification |

## Open blockers
- Mandatory retained hardware evidence is still incomplete under `evidence/<YYYYMMDD>/<scenario-id>/`.
- Browser config CRUD retained evidence is still failing even though `config.get_capabilities` is now proven on hardware.
- Web lint is intentionally not part of enforced PR CI until the ESLint patch incompatibility is resolved by `2026-04-20`.
- Host CI does not substitute for real USB/BLE/flash/config hardware validation.
