# CURRENT_TASK.md

## Active Task
- ID: REL-HW-001
- Title: Release-readiness reassessment after partial retained hardware evidence
- Status: in_progress

## Why this is the active task

This branch has already completed the major implementation lanes:

- compiler-backed serial-first `@CFG:` `v2` config/apply
- dual-profile firmware output support:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- dual-profile web companion Config UX
- PR CI enforcement for firmware plus web test/build lanes
- documentation alignment for the implemented branch truth

The current retained evidence state is mixed rather than absent:

- shell-side `config.get_capabilities` transport proof now passes reproducibly on the current flashed image
- browser `Capabilities` now succeeds on real hardware
- browser `Persist`, `Clear`, and the dependent `Load` steps are still failing in retained evidence
- most mandatory USB/BLE/OPS/STG/REL scenarios still have not been executed to retained completion

The primary remaining release blocker is therefore incomplete and mixed physical evidence, plus the separately documented lint-stability follow-up.

## Exit criteria

- Mandatory scenarios in `HARDWARE_VALIDATION_PACK.md` have retained evidence under `evidence/<YYYYMMDD>/<scenario-id>/`
- Release-facing docs explicitly state that:
  - `config.get_capabilities` is hardware-proven on the current flashed image
  - browser `persist/load/clear/load` is not yet hardware-proven
  - the overall release gate is still blocked
- Release docs and checklists can point to concrete evidence paths
- No doc drifts back to pre-implementation statements about single-profile support, compiler planning, or missing PR web CI
- The deferred lint follow-up remains documented in `CI_WEB_ENFORCEMENT_REPORT.md` with target date `2026-04-20`
