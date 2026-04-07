# CFG Transport Isolation Matrix Postfix

Date: 2026-04-07
Repo: `USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD under test: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Scope

Goal: convert the post-fix transport result from a one-off success into retained, repeatable proof.

Fresh image provenance for this matrix:

- current branch build flashed via `evidence/20260407/CFG-TRANSPORT-POSTFIX/artifacts/20260407T084916Z-idf-flash-current-build.log`

Visible host serial candidates during this run:

- `/dev/cu.wchusbserial5B5E0200881`
- `/dev/cu.usbmodem5B5E0200881`

Host metadata shows those are aliases for the same USB serial device, not two distinct hardware transports:

- `VID:PID=1A86:55D3`
- `serial=5B5E020088`
- `location=20-6`

See `evidence/20260407/CFG-TRANSPORT-POSTFIX/artifacts/20260407T084949Z-host-port-metadata.log`.

## Result Summary

### Path A raw shell probe

Pass on both visible host aliases.

- exact framed `config.get_capabilities` request sent
- `114` bytes sent on each alias
- `319` bytes received on each alias
- `has_cfg_frame=true` on each alias
- retained request-to-first-frame latency:
  - WCH alias: `202.69 ms`
  - usbmodem alias: `200.85 ms`

### Path B repro tool probe

Pass on both visible host aliases.

- existing `scripts/repro_cfg_timeout.py` succeeded on each alias
- tool summary retained `frame_len=114`, `response_bytes=319`, `has_cfg_frame=true`
- retained wrapper latency:
  - WCH alias: `202.2 ms`
  - usbmodem alias: `204.87 ms`

### Path C browser flow

Partial pass only.

- `config.get_capabilities` succeeded
- full CRUD roundtrip did not succeed
- `config.persist` timed out
- `config.load` failed with `kContractViolation (reason 0)`
- `config.clear` timed out
- `config.load` after clear failed again with `kContractViolation (reason 0)`

Browser details are retained separately in `browser-roundtrip-proof.md`.

## Matrix

| Port candidate | Path | Command used | Open parameters | Bytes sent | Bytes received | `@CFG` frame | Latency ms | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `/dev/cu.wchusbserial5B5E0200881` | A raw shell probe | inline `pyserial` exact `config.get_capabilities` frame | `baud=115200`, `timeout=0.2`, `write_timeout=1`, `DTR=false`, `RTS=false`, boot wait `8.0s`, settle `0.5s`, response timeout `4.0s` | `114` | `319` | `true` | `202.69` | pass |
| `/dev/cu.usbmodem5B5E0200881` | A raw shell probe | inline `pyserial` exact `config.get_capabilities` frame | `baud=115200`, `timeout=0.2`, `write_timeout=1`, `DTR=false`, `RTS=false`, boot wait `8.0s`, settle `0.5s`, response timeout `4.0s` | `114` | `319` | `true` | `200.85` | pass |
| `/dev/cu.wchusbserial5B5E0200881` | B repro tool | `python -u scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --request-id 9601 --wait-for-boot` | tool defaults: `baud=115200`, `timeout=0.2`, `write_timeout=1`, `DTR=false`, `RTS=false`, boot wait enabled | `114` | `319` | `true` | `202.2` | pass |
| `/dev/cu.usbmodem5B5E0200881` | B repro tool | `python -u scripts/repro_cfg_timeout.py --port /dev/cu.usbmodem5B5E0200881 --command get_capabilities --request-id 9602 --wait-for-boot` | tool defaults: `baud=115200`, `timeout=0.2`, `write_timeout=1`, `DTR=false`, `RTS=false`, boot wait enabled | `114` | `319` | `true` | `204.87` | pass |

## Browser Readout

Browser proof source:

- `browser-roundtrip-proof.md`

What the browser run proves:

- the post-fix browser can now fetch capabilities successfully against the current flashed image
- the selected chooser entry was `USB Single Serial (cu.wchusbserial5B5E0200881)`
- the remaining failures are command-specific and occur after transport establishment, not before it

What the browser run does not prove:

- successful end-to-end persist/load/clear/load roundtrip

## Evidence Notes

Superseded artifact:

- `evidence/20260407/CFG-TRANSPORT-POSTFIX/artifacts/20260407T085006Z-raw-probe-wch-get-capabilities.log`

Reason it is excluded from the passing matrix:

- that probe accidentally sent a literal backslash-`n` sequence instead of a newline terminator, producing `115` sent bytes and no reply
- the corrected `v2` raw probes are the valid retained artifacts

Likewise, the earlier repro wrapper artifacts with impossible latency are excluded in favor of the byte-accurate `v3` artifacts:

- `evidence/20260407/CFG-TRANSPORT-POSTFIX/artifacts/20260407T085203Z-repro-tool-wch-get-capabilities-v3.log`
- `evidence/20260407/CFG-TRANSPORT-POSTFIX/artifacts/20260407T085219Z-repro-tool-usbmodem-get-capabilities-v3.log`

## Bottom Line

Post-fix transport binding proof is now reproducible for `config.get_capabilities`:

- raw shell probe passes
- repro tool passes
- both visible macOS serial aliases pass
- browser `Capabilities` passes

The remaining red area is not basic transport binding. It is the command-specific browser roundtrip beyond `get_capabilities`, with `persist` and `clear` timing out and `load` failing with `kContractViolation (reason 0)` after the failed mutate step.
