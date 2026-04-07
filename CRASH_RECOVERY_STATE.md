# CRASH_RECOVERY_STATE.md

## Historical Note

This file is a point-in-time crash recovery handoff from earlier in the branch.

For the current branch truth and release status, use:
- `UPDATED_MILESTONE_STATUS.md`
- `CURRENT_TASK.md`
- `browser-roundtrip-proof.md`

## Purpose

Rebuild operational context after the crash without reopening completed implementation milestones.

## Repo Identity

- Requested repo shorthand: `/USB2BLE`
- Actual live repo root in this environment: `/Users/alex/Developer/CodexUSB/USB2BLE`
- Branch: `codex/freeze-contract-plan`
- HEAD: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Context Snapshot

### Git state

- Working tree is heavily in progress and should be treated as intentional branch work, not cleanup noise.
- Current `git status --short` summary:
  - `69` modified
  - `7` deleted
  - `30` untracked
- Do not reset or reopen completed milestones from this state.

### Latest commits

```text
ce0023b (HEAD -> codex/freeze-contract-plan, origin/main, origin/HEAD) Merge pull request #14 from alexoviedo/codex/interactive-flash-console-debug
c9390c7 (origin/codex/interactive-flash-console-debug, codex/interactive-flash-console-debug) fix: unblock firmware build and safe console attach
a689000 Merge pull request #13 from alexoviedo/codex/fix-quiet-preferred-runtime-port-auto-attach
2240e09 (origin/codex/fix-quiet-preferred-runtime-port-auto-attach, main, codex/fix-quiet-preferred-runtime-port-auto-attach) fix: keep preferred quiet runtime ports attached
113937a Merge pull request #12 from alexoviedo/codex/fix-score-penalty-for-flash-context
b4e2eb9 (origin/codex/fix-score-penalty-for-flash-context) Fix serial port candidate scoring and quiet-request preference
828d776 Merge pull request #11 from alexoviedo/codex/fix-serial-lifecycle-reliability-issues
dd7f188 (origin/codex/fix-serial-lifecycle-reliability-issues) Harden serial candidate viability and lifecycle ownership logging
```

### Key docs and status files present

- Present:
  - `CURRENT_TASK.md`
  - `TODO.md`
  - `HARDWARE_VALIDATION_PACK.md`
  - `VALIDATION.md`
  - `INTEGRATION_DELTA.md`
  - `UPDATED_MILESTONE_STATUS.md`
  - `DOC_TRUTH_ALIGNMENT_REPORT.md`
  - `scripts/hardware_evidence.sh`

### Evidence inventory

Current retained evidence exists only for the bootstrap snapshot:

```text
evidence/
evidence/20260406/
evidence/20260406/ENV-BOOTSTRAP/
evidence/20260406/ENV-BOOTSTRAP/artifacts/
evidence/20260406/ENV-BOOTSTRAP/commands.txt
evidence/20260406/ENV-BOOTSTRAP/result.json
```

`ENV-BOOTSTRAP` currently contains `9` bounded per-step artifact logs and is about `40K`, replacing the earlier runaway multi-gigabyte transcript.

## Expected Artifact Check

### `INTEGRATION_DELTA.md`

- Exists.
- Contents summarize an earlier integration-gap analysis.
- Important caution:
  - this file still describes several items as missing that newer branch-state docs now mark complete
  - examples include BLE-facing profile work, dual-profile web exposure, and PR CI web enforcement
- Use this as historical planning context only, not as the current source of truth for resume.

### `UPDATED_MILESTONE_STATUS.md`

- Exists.
- This is the current operational status source.
- Current meaning:
  - `M0` through `M5` are complete
  - `M6` is partial only because retained hardware evidence is still missing
  - locally verified baseline is:
    - firmware CTest `20/20`
    - web Vitest `6/6` files, `59` passed, `1` skipped
    - web production build passing
- Explicit guardrail:
  - do not reopen USB ingestion, compiler, profile-manager/encoder/BLE contract, web profile exposure, or PR CI enforcement unless a narrow regression is found

### `DOC_TRUTH_ALIGNMENT_REPORT.md`

- Exists.
- Contents record the doc-alignment pass that updated the current truth-bearing files.
- Main result:
  - docs were updated away from stale single-profile or pre-implementation language
  - release/hardware evidence is now framed as the main remaining blocker

### `scripts/hardware_evidence.sh`

- Exists.
- Purpose:
  - `bootstrap <date>` captures a bounded environment snapshot
  - `init <date> <scenario>` creates `evidence/<date>/<scenario>/`
  - `record <date> <scenario> <label> -- <command...>` appends command metadata to `commands.txt` and writes stdout/stderr to `artifacts/<timestamp>-<label>.log`
- This script is the preferred evidence-capture workflow because it keeps `commands.txt` readable and prevents another runaway transcript.

## What Is Done

Per `UPDATED_MILESTONE_STATUS.md`, the completed implementation lanes are already in place and should not be redone:

- USB ingestion hardening
- Production config compiler and runtime-effective apply
- Serial-first `@CFG:` `v2` config/apply flow
- Dual-profile firmware output support
- Dual-profile web Config UX
- PR CI enforcement for firmware plus web test/build lanes
- Truth-aligned status and validation docs

Additional recovery work already completed after the crash:

- Rebuilt `evidence/20260406/ENV-BOOTSTRAP/` with bounded logs
- Confirmed the expected recovery artifacts exist
- Reconfirmed the locally passing automated baseline:
  - `cmake --build build/unit --parallel`
  - `ctest --test-dir build/unit --output-on-failure`
  - `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
  - `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`

## What Remains

The only remaining release-closure work is hardware evidence and the already-deferred lint follow-up.

### Hardware evidence still missing

Mandatory scenarios still need retained evidence under `evidence/<YYYYMMDD>/<scenario-id>/`:

- `CFG-01`
- `CFG-02`
- `CFG-03`
- `USB-01`
- `USB-02`
- `BLE-01`
- `BLE-02`
- `BLE-03`
- `OPS-01`
- `STG-01`
- `REL-01`
- `REL-02`

### Deferred maintenance item

- Web lint stabilization remains deferred until `2026-04-20`
- Current blocker:
  - `@rushstack/eslint-patch` fails before repo lint rules execute under ESLint 9
- Source of truth:
  - `CI_WEB_ENFORCEMENT_REPORT.md`

## Current Known Blockers

### Serial device visibility

- This is not currently blocked.
- Live probe in the current shell shows:
  - `/dev/cu.usbmodem5B5E0200881`
  - `/dev/cu.wchusbserial5B5E0200881`
- Note:
  - the earlier `ENV-BOOTSTRAP` snapshot recorded no visible serial ports at `2026-04-07T02:06:57Z`
  - that earlier blocker appears cleared now, but visibility should still be rechecked immediately before hardware steps

### ESP-IDF toolchain setup

- Still blocked in the current shell.
- Current probe:
  - `idf.py` is not on `PATH`
  - no `IDF*` or `ESP*` environment variables are set in this shell
- Good news:
  - `/Users/alex/esp/esp-idf/export.sh` exists
  - `~/.espressif/` also exists, so the local install looks present
- Operational meaning:
  - source the ESP-IDF export script before any `idf.py` build/flash step

## Exact Next Command Sequence

Use this sequence to resume the hardware-evidence lane from the current repo state.

### Terminal A: restore ESP-IDF and prepare `OPS-01`

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE
. /Users/alex/esp/esp-idf/export.sh
idf.py --version
ls /dev/cu.* 2>/dev/null | rg 'usb|wch|modem|serial|SLAB|UART'
scripts/hardware_evidence.sh init 20260406 OPS-01
scripts/hardware_evidence.sh record 20260406 OPS-01 idf-version -- idf.py --version
scripts/hardware_evidence.sh record 20260406 OPS-01 firmware-build -- idf.py build
```

### Terminal B: start the web companion for flash and console steps

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE/charm-web-companion
PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run dev
```

### Terminal C: prepare serial capture for the post-flash console attach

Choose the active device path visible at runtime. Current candidates are:
- `/dev/cu.usbmodem5B5E0200881`
- `/dev/cu.wchusbserial5B5E0200881`

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE
PORT=/dev/cu.wchusbserial5B5E0200881
python -m serial.tools.miniterm "$PORT" 115200 | tee evidence/20260406/OPS-01/serial.log
```

### Browser step between Terminal B and Terminal C

Use the running web companion to execute the real `OPS-01` flow:

1. Open the Flash view.
2. Flash the current firmware artifact set.
3. Confirm reboot.
4. Open the Console view and attach.
5. Verify normal boot logs rather than ROM download mode.
6. Fill in `evidence/20260406/OPS-01/result.json` after the scenario passes or fails.

## Resume Guardrails

- Do not reopen completed implementation milestones.
- Do not delete or reset the current worktree.
- Use `UPDATED_MILESTONE_STATUS.md` and `CURRENT_TASK.md` as the current-state resume sources.
- Treat `INTEGRATION_DELTA.md` as historical planning context when it conflicts with newer status files.
