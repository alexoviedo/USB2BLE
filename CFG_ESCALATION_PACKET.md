# CFG Escalation Packet

Historical note:

- This packet freezes the escalation state before the later audited reflash and postfix repro pass.
- It remains useful as incident history, but it is not the latest branch truth.
- For the current retained state, prefer:
  - `CFG_TRANSPORT_BINDING_AUDIT.md`
  - `CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md`
  - `browser-roundtrip-proof.md`

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD under test: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Purpose

Freeze the repeated post-fix `CFG-01` failure into a handoff packet that:

- points to the current retained evidence
- records the `CFG-02` stop condition
- summarizes the attempt/fix timeline to date
- converts the incident into objective, falsifiable hypotheses

## Current Escalation State

- `CFG-01`: `FAIL`
- `CFG-02`: `PENDING` and explicitly blocked by protocol after the renewed `CFG-01` failure
- Release gate status: blocked
- Escalation reason: the browser path still times out after the web transport fix, and the latest serial-side repro still shows boot completion with no `@CFG:` response frame

## Latest Retained Evidence

### Latest `CFG-01` evidence

- Scenario result:
  - `evidence/20260407/CFG-01/result.json`
- Operator narrative:
  - `evidence/20260407/CFG-01/scenario-notes.md`
- Retained serial log:
  - `evidence/20260407/CFG-01/serial.log`
- Recorded host precheck:
  - `evidence/20260407/CFG-01/artifacts/20260407T074421Z-host-precheck.log`
- Latest post-fix shell repro:
  - `evidence/20260407/CFG-01/artifacts/20260407T075255Z-post-fix-shell-repro-get-capabilities.log`

### Latest `CFG-01` failure statement

Browser observation from the retained notes:

- normal session: `Command config.get_capabilities timed out (ID: 1)`
- Incognito with explicit `USB Single Serial (cu.wchusbserial5B5E0200881)` selection: `Command config.get_capabilities timed out (ID: 1)`

Latest serial-side retained summary from `20260407T075255Z-post-fix-shell-repro-get-capabilities.log`:

- `BOOT_PHASE_DONE True`
- `REQUEST_SENT`
- `RESPONSE_PHASE_DONE`
- `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.get_capabilities", "frame_len": 114, "has_cfg_frame": false, "port": "/dev/cu.wchusbserial5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`

Interpretation:

- the board boots
- the request is sent after boot completes
- no `@CFG:` frame is retained on the active tested serial path

### `CFG-02` stop condition

- Scenario result:
  - `evidence/20260407/CFG-02/result.json`
- Stop-condition notes:
  - `evidence/20260407/CFG-02/scenario-notes.md`
- Recorded blocker artifact:
  - `evidence/20260407/CFG-02/artifacts/20260407T075321Z-blocked-after-cfg-01-fail.log`

Frozen stop statement:

- `CFG-02` was not rerun on April 7, 2026 because `CFG-01` failed post-fix
- per protocol, execution stopped and returned to the T2/T3 loop
- no USB/BLE gates should proceed from this state

## Timeline

### April 7, 2026: baseline and initial incident capture

1. Environment readiness was restored and re-proved:
   - `ENV_READY_REPORT.md`
   - `evidence/20260407/ENV-READY/`
2. Initial hardware evidence run failed:
   - `CFG-01`: browser `config.get_capabilities` timed out
   - `CFG-02`: browser `config.persist` timed out
3. Shell probes and retained serial logs showed:
   - normal boot logs
   - no retained `@CFG:` response frame
4. The incident was frozen in:
   - `CFG_INCIDENT_REPORT.md`

### April 7, 2026: structured diagnosis

1. Diagnosis matrix was built in:
   - `CFG_DIAG_MATRIX.md`
2. Minimum repro was reduced to:
   - `scripts/repro_cfg_timeout.py`
3. Strongest diagnosis result:
   - even after boot completes and DTR/RTS are deasserted, direct serial repro still yields `response_bytes: 0`

### April 7, 2026: smallest safe fix attempt

1. Narrow fix applied only to the web config transport:
   - `charm-web-companion/lib/adapters/SerialConfigTransport.ts`
2. Added targeted regression tests for:
   - immediate `get_capabilities` response handling
   - `persist` request/response roundtrip
   - timeout/no-response cleanup
3. Verification passed:
   - `20/20` unit tests
   - web Vitest `60/60`
   - web production build
4. Fix write-up recorded in:
   - `CFG_FIX_REPORT.md`

### April 7, 2026: post-fix hardware rerun

1. Fresh `CFG-01` rerun loaded the Config UI successfully
2. Browser still timed out:
   - normal session: no chooser, timeout
   - Incognito: explicit WCH port selection, same timeout
3. Fresh post-fix shell repro still showed:
   - boot completes
   - request is sent
   - `response_bytes: 0`
   - `has_cfg_frame: false`
4. `CFG-02` was blocked and not rerun

## Ranked Hypotheses

Ranking is based on the latest retained evidence from April 7, 2026, including the post-fix rerun.

### Rank 1: H3 firmware runtime path not active on flashed image

Hypothesis:
- the flashed image boots normally, but the live config transport runtime is not actually receiving from or replying on the tested UART request path

Why it currently ranks first:
- the latest direct serial repro removes the browser from the equation and still yields zero response bytes after boot completes
- service/runtime unit tests imply that if a valid request reaches the runtime adapter/service, some reply should be emitted
- the latest failure survives the web transport fix

Evidence:
- `CFG_DIAG_MATRIX.md`
- `evidence/20260407/CFG-01/artifacts/20260407T075255Z-post-fix-shell-repro-get-capabilities.log`
- `evidence/20260407/CFG-01/serial.log`

Falsifiable test:
- instrument the flashed firmware image to emit an unambiguous runtime-start marker and a request-received marker from the config transport task on the same UART path, then rerun `scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --wait-for-boot`

Pass/fail criterion:
- if the markers appear and an `@CFG:` response still does not appear, the hypothesis weakens
- if the markers never appear on the flashed image while boot logs do appear, the hypothesis strengthens strongly

### Rank 2: H2 wrong serial interface or line settings

Hypothesis:
- the current requests are being sent to the wrong effective input interface, or the line-control/open behavior still leaves the board on the wrong side of a reset/strap window for the active runtime path

Why it currently ranks second:
- the board exposes both `cu.wchusbserial...` and `cu.usbmodem...`
- `sdkconfig` uses UART0 as primary console and USB Serial/JTAG as secondary output, which is consistent with interface ambiguity
- however, the latest tested WCH path still shows post-boot zero-response behavior, so this is not stronger than H3

Evidence:
- `CFG_DIAG_MATRIX.md`
- `evidence/20260407/CFG-01/scenario-notes.md`
- `sdkconfig` console settings referenced in `CFG_DIAG_MATRIX.md`

Falsifiable test:
- perform an interface-isolation run where only one host-visible interface is used at a time and capture both:
  - a direct post-boot request on `/dev/cu.wchusbserial5B5E0200881`
  - a direct post-boot request on `/dev/cu.usbmodem5B5E0200881`
  with explicit DTR/RTS state recording and identical timing

Pass/fail criterion:
- if exactly one interface yields an `@CFG:` reply under controlled line settings, H2 strengthens
- if neither interface yields any reply after controlled post-boot sends, H2 weakens relative to H3

### Rank 3: H1 browser transport path issue

Hypothesis:
- the browser path still has a transport defect that causes the command to time out before a valid device reply is observed

Why it currently ranks third:
- a real browser-side race did exist and was fixed
- the browser still times out
- but the latest direct serial repro still gets zero response bytes, so browser behavior alone cannot explain the current retained failure

Evidence:
- `CFG_FIX_REPORT.md`
- `evidence/20260407/CFG-01/scenario-notes.md`
- `evidence/20260407/CFG-01/artifacts/20260407T075255Z-post-fix-shell-repro-get-capabilities.log`

Falsifiable test:
- capture Web Serial lifecycle logs plus raw incoming chunks during a single `Capabilities` click, and compare them to a simultaneous external serial capture on the same run

Pass/fail criterion:
- if the external capture shows a valid `@CFG:` reply while the browser still times out or drops it, H1 strengthens
- if the external capture also shows no reply bytes at all, H1 weakens

### Rank 4: H4 request framing mismatch on-wire

Hypothesis:
- the bytes sent over the real wire differ from the intended `@CFG:` frame in a way that causes the firmware to ignore the request

Why it currently ranks fourth:
- tests already verify the intended request framing
- live frame sizes are small and valid
- parser/service behavior suggests malformed frames should typically trigger explicit rejection frames rather than total silence
- this remains possible only if the real on-wire bytes differ from the logical request object

Evidence:
- `CFG_DIAG_MATRIX.md`
- `charm-web-companion/__tests__/config.test.ts`
- `scripts/repro_cfg_timeout.py`

Falsifiable test:
- capture the exact transmitted bytes on the live wire for `config.get_capabilities` using a serial tap or host-side byte-logging shim, then compare them byte-for-byte against the expected frame:
  - `@CFG:{"protocol_version":2,"request_id":9401,"command":"config.get_capabilities","payload":{},"integrity":"CFG1"}\n`

Pass/fail criterion:
- if the transmitted bytes differ materially from the expected frame, H4 strengthens
- if the transmitted bytes match exactly and no reply follows, H4 weakens

## Recommended Next Diagnostic Order

1. H3 test first
   - it has the highest explanatory power and best matches the post-fix direct serial repro
2. H2 test second
   - it cleanly separates runtime inactivity from interface/path mismatch
3. H1 test third
   - only after we know whether any valid reply exists externally
4. H4 test fourth
   - only if the first three do not explain the silent path

## Escalation Summary

Current best objective statement:

- as of April 7, 2026, the repeated `CFG-01` failure is no longer just a browser symptom
- the latest retained post-fix evidence shows:
  - successful boot
  - request sent after boot completion
  - zero retained response bytes
  - no `@CFG:` frame
- `CFG-02` is correctly frozen as blocked, not rerun

This packet should be the starting point for the next T2/T3 investigation loop.
