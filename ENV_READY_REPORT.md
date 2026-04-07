# ENV_READY_REPORT.md

## Historical Note

This file records the environment recovery pass from earlier on April 7, 2026.

For the latest branch-level validation and release status, use:
- `UPDATED_MILESTONE_STATUS.md`
- `VALIDATION.md`
- `browser-roundtrip-proof.md`

## Purpose

Document the shell/tooling recovery that cleared the `idf.py` blocker and re-proved the baseline before the next hardware session.

## Environment Recovery

- Repo root: `/Users/alex/Developer/CodexUSB/USB2BLE`
- Branch: `codex/freeze-contract-plan`
- Commit: `ce0023b`
- ESP-IDF activation command:
  - `source /Users/alex/esp/esp-idf/export.sh`
- Verified `idf.py` path:
  - `/Users/alex/esp/esp-idf/tools/idf.py`
- Verified `idf.py` version:
  - `ESP-IDF v5.5.3`

## Evidence Location

- Scenario root:
  - `evidence/20260407/ENV-READY/`
- Command trace:
  - `evidence/20260407/ENV-READY/commands.txt`
- Result record:
  - `evidence/20260407/ENV-READY/result.json`

## Pass/Fail Table

| Step | Command | Result | Duration (`real`) | Evidence log | Key output |
| --- | --- | --- | --- | --- | --- |
| 1 | `which idf.py` after sourcing `/Users/alex/esp/esp-idf/export.sh` | PASS | `0.00s` | `evidence/20260407/ENV-READY/artifacts/20260407T022019Z-which-idf.log` | `/Users/alex/esp/esp-idf/tools/idf.py` |
| 2 | `idf.py --version` after sourcing `/Users/alex/esp/esp-idf/export.sh` | PASS | `2.53s` | `evidence/20260407/ENV-READY/artifacts/20260407T022024Z-idf-version.log` | `ESP-IDF v5.5.3` |
| 3 | `cmake --build build/unit --parallel` | PASS | `0.88s` | `evidence/20260407/ENV-READY/artifacts/20260407T022031Z-cmake-build.log` | Unit targets rebuilt successfully |
| 4 | `ctest --test-dir build/unit --output-on-failure` | PASS | `0.48s` | `evidence/20260407/ENV-READY/artifacts/20260407T022033Z-ctest.log` | `20/20` tests passed |
| 5 | `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run` | PASS | `10.01s` | `evidence/20260407/ENV-READY/artifacts/20260407T022034Z-web-vitest.log` | `6/6` files passed, `59` passed, `1` skipped |
| 6 | `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build` | PASS | `32.94s` | `evidence/20260407/ENV-READY/artifacts/20260407T022044Z-web-build.log` | Next.js production build and export passed |

## Outcome

- The shell-level `idf.py` blocker is cleared when the ESP-IDF export script is sourced.
- The firmware and web automated baseline re-passed from the current branch state.
- No remediation was needed because all commands succeeded on the first recorded run.

## Ready State

The repo is ready to continue into retained hardware evidence collection.

Recommended next move:
- start `OPS-01` or another mandatory hardware scenario under `evidence/20260407/<scenario-id>/` using `scripts/hardware_evidence.sh`
