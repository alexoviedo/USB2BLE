# DOC_TRUTH_ALIGNMENT_REPORT.md

## Purpose

Record the documentation files changed in this truth-alignment pass and why they were updated.

## Scope

- Branch: `codex/freeze-contract-plan`
- Intent: eliminate stale pre-implementation language and align project/user-facing docs with the current implemented branch truth
- Behavior changes: none

## Changed Files

| File | Rationale |
| --- | --- |
| `README.md` | Replaced stale single-profile language with the implemented dual-profile truth, added current PR CI web enforcement language, and kept the release gate centered on retained hardware evidence. |
| `ARCHITECTURE.md` | Updated root architecture status for implemented dual-profile BLE-facing support, compiler-backed `v2` config persistence, and the current CI-vs-hardware validation boundary. |
| `CURRENT_TASK.md` | Moved the active-task framing from docs cleanup to the real remaining release lane: retained hardware evidence closure. |
| `TODO.md` | Converted the backlog from pre-implementation work into a current-state backlog with completed implementation items, hardware evidence as the main remaining blocker, and lint stabilization as a dated deferred item. |
| `VALIDATION.md` | Updated the automated proof section to reflect enforced PR web CI, Node 20 command reality, and the remaining hardware-only proof boundaries. |
| `HARDWARE_VALIDATION_PACK.md` | Replaced stale `v1` config wording with the current `v2` contract language and aligned the release-gate scenarios with the implemented compiler-backed, dual-profile product lane. |
| `charm-web-companion/README.md` | Aligned the web companion’s product truth with implemented dual-profile config selection, enforced PR web CI, and hardware-proof release limits. |
| `charm-web-companion/ARCHITECTURE.md` | Replaced roadmap-only profile wording with the implemented two-profile contract and clarified that `get_capabilities` is not a runtime profile registry. |
| `charm-web-companion/HANDOFF.md` | Updated the handoff snapshot so it reflects the shipped dual-profile/compiler lane and the real remaining release blocker instead of roadmap language. |
| `FEATURE_MATRIX.md` | Rebuilt the feature/status matrix around the current branch state so compiler, dual-profile support, web CI enforcement, and remaining release gaps are represented honestly. |
| `UPDATED_MILESTONE_STATUS.md` | Replaced outdated milestone blockers with the current state: implementation lanes complete, release evidence still pending, and web lint explicitly deferred. |
| `PROFILE_OUTPUT_MATRIX.md` | Removed the stale Generic-only UI limit and aligned the honest-limits section with the current dual-profile web Config UX. |
| `TEST_STRATEGY.md` | Rewrote the test strategy from future-tense implementation planning into a current-state proof strategy centered on automated coverage plus retained hardware evidence. |

## Main Truth Corrections

1. The branch no longer claims “one concrete profile encoder path”; both `Generic BLE Gamepad` and `Wireless Xbox Controller` are implemented and documented as the supported profile set.
2. Config transport docs now consistently describe the current serial-first `@CFG:` `v2` contract and compiler-backed runtime apply path.
3. The web companion docs now consistently describe the current constrained profile selector for `profile_id = 1` and `profile_id = 2`, while preserving the boundary that `config.get_capabilities` does not enumerate profiles.
4. CI documentation now consistently states that PRs enforce the web quality lane, with web lint explicitly deferred and dated.
5. Release and validation docs now consistently show retained hardware evidence as the main remaining release blocker.

## Validation Approach

- Searched key docs for stale terms such as:
  - `one concrete profile`
  - `planned`
  - `Generic-only`
  - `single-profile`
  - `PR CI still does not enforce`
  - `v1`
- Re-aligned the release checklist and hardware pack against current implemented behavior.
- Left historical planning documents out of this pass where they are not user-facing status sources; the files above are the current truth-bearing set.
