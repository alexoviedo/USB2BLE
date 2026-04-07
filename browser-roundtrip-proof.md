# Browser Roundtrip Proof

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD under test: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Scope

Goal: retain a human-observed browser proof for the post-fix config flow at `http://localhost:3000/USB2BLE`.

Browser reachability precheck:

- [web-root-check.log](/Users/alex/Developer/CodexUSB/USB2BLE/evidence/20260407/CFG-BROWSER-ROUNDTRIP/artifacts/20260407T085246Z-web-root-check.log)

User-provided screenshot sources for this proof:

- [Screen Shot 2026-04-07 at 2.55.03 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.55.03%20AM.png)
- [Screen Shot 2026-04-07 at 2.55.24 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.55.24%20AM.png)
- [Screen Shot 2026-04-07 at 2.55.45 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.55.45%20AM.png)
- [Screen Shot 2026-04-07 at 2.56.15 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.56.15%20AM.png)
- [Screen Shot 2026-04-07 at 2.56.40 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.56.40%20AM.png)
- [Screen Shot 2026-04-07 at 2.56.59 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.56.59%20AM.png)
- [Screen Shot 2026-04-07 at 2.57.25 AM.png](/Users/alex/Desktop/Screen%20Shot%202026-04-07%20at%202.57.25%20AM.png)

## Selected Port

Chooser result:

- `USB Single Serial (cu.wchusbserial5B5E0200881)`

The chooser screenshot also showed `USB Single Serial (cu.usbmodem5B5E0200881)` as another host alias, consistent with the shell-side metadata that both aliases refer to the same USB serial device.

## Action Sequence

| Step | Action | Result | Exact observed status |
| --- | --- | --- | --- |
| 1 | `Capabilities` | PASS | `Fetched device config capabilities.` |
| 2 | `Persist to Device` | FAIL | `Command config.persist timed out (ID: 2)` |
| 3 | `Load Current` | FAIL | `config.load failed: kContractViolation (reason 0)` |
| 4 | `Clear Device Config` | FAIL | confirmation dialog `Clear device configuration?`, then `Command config.clear timed out (ID: 4)` |
| 5 | `Load Current` after clear | FAIL | `config.load failed: kContractViolation (reason 0)` |

## Supporting Observations

### Successful capability fetch

Retained from the successful first action:

- `Device Capabilities` panel rendered
- `Protocol: v2`
- `Persist: YES`
- `Load: YES`
- `BLE Transport: NO`

This confirms the browser can establish the serial session and receive at least one framed `@CFG` response on the current flashed image.

### Persist failure

Persist did not complete after chooser selection of the WCH alias.

Observed result:

- `Command config.persist timed out (ID: 2)`

### Load failure after failed persist

The first `Load Current` did not time out. It returned a contract-level failure:

- `config.load failed: kContractViolation (reason 0)`

That is consistent with the device having no successfully persisted config to load after the failed persist step.

### Clear failure

`Clear Device Config` displayed the expected confirmation dialog first:

- `Clear device configuration?`

After confirmation, the command timed out:

- `Command config.clear timed out (ID: 4)`

### Load-after-clear failure

The final `Load Current` repeated the same contract-level failure:

- `config.load failed: kContractViolation (reason 0)`

## Interpretation

This browser run is a retained partial proof, not a full roundtrip proof.

What is proven:

- browser `Capabilities` now succeeds on the current flashed image
- chooser selection and transport establishment are good enough for a read-only config query

What is not proven:

- successful browser-side `persist -> load -> clear -> load` roundtrip

Current browser roundtrip outcome:

- `1/5` actions passed
- `4/5` actions failed

## Bottom Line

The current post-fix state is:

- basic browser config transport is reproducibly alive
- full browser config CRUD roundtrip is still not reproducibly healthy

The remaining defect is now narrowed to the command-specific path after capability fetch, not the initial serial binding or capability query path.
