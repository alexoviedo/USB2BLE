# DOC_ALIGNMENT_REPORT.md

## Purpose
Record every documentation file changed in this alignment pass and why it was updated.

## Scope

- Branch: `codex/freeze-contract-plan`
- Intent: docs-only alignment for repository scope, config contract truth, profile roadmap, and hard release gating
- Behavior changes: none

## Changed Docs

| File | Why it changed |
|---|---|
| `README.md` | Reframed the repo as a combined firmware + web companion product, clarified the current config truth, called out the Generic+Xbox roadmap, and made retained hardware evidence part of release criteria. |
| `ARCHITECTURE.md` | Expanded the root architecture view to include the web companion, clarified current config behavior versus roadmap guardrails, and added explicit profile/release-boundary language. |
| `CURRENT_TASK.md` | Replaced stale firmware-only cleanup framing with the active documentation-alignment mission for the combined product direction. |
| `TODO.md` | Reworked the backlog from firmware-only tasks into an integrated roadmap covering docs, CI, compiler, profiles, config/apply, and hardware release gating. |
| `VALIDATION.md` | Distinguished current automated proof from hardware-required proof and made retained hardware evidence an explicit release blocker. |
| `HARDWARE_VALIDATION_PACK.md` | Expanded the scenario matrix to reflect the active production lane, including compiler/apply and the Generic+Xbox profile roadmap, and clarified that missing evidence blocks release. |
| `CONFIG_TRANSPORT_CONTRACT.md` | Separated the currently implemented `v1` behavior from the guardrails for future compiler-backed serial-first expansion, without claiming unsupported transport behavior today. |
| `charm-web-companion/README.md` | Clarified the web companion’s current firmware-backed truth, preserved honest config boundaries, and aligned its release checklist with the hard hardware gate. |
| `charm-web-companion/ARCHITECTURE.md` | Fixed serial ownership language, clarified current config limits, and aligned the profile/compiler roadmap with the no-fake-capability rule. |
| `charm-web-companion/HANDOFF.md` | Replaced stale “final handoff” language and speculative future features with a current repository snapshot that matches the active firmware + web companion direction. |

## Main Contradictions Resolved

1. The repo is no longer described as firmware-only while simultaneously shipping and deploying the web companion.
2. The config contract docs now distinguish current implemented `v1` behavior from future compiler-backed work instead of mixing them together.
3. Profile documentation now states the current implemented generic path and the planned `Generic BLE Gamepad` + `Wireless Xbox Controller` roadmap without pretending Xbox support already exists.
4. Validation and release docs now state clearly that hardware evidence is a hard release gate, not a follow-up task after artifacts are built.

## Notes

- No source-code behavior was changed in this pass.
- Planning docs created earlier on this branch were not modified here because they already matched the amended target scope.
