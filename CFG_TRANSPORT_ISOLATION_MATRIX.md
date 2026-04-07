# CFG Transport Isolation Matrix

Historical note:

- This matrix captures the pre-reflash isolation state earlier on April 7, 2026.
- It was superseded by the later postfix repro pass after reflashing the current branch build.
- Use the current retained state here instead:
  - `CFG_TRANSPORT_BINDING_AUDIT.md`
  - `CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md`
  - `browser-roundtrip-proof.md`

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD under test: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Scope

Goal: isolate whether `config.get_capabilities` failure is primarily in the browser path, serial interface selection/line settings, or the firmware-side config command runtime.

Visible serial candidates at test time:

- `/dev/cu.wchusbserial5B5E0200881`
- `/dev/cu.usbmodem5B5E0200881`

Paths exercised:

- Path A: raw wire probe via shell / `pyserial`
- Path B: existing repro tool `scripts/repro_cfg_timeout.py`
- Path C: webapp `Config -> Capabilities`

## Result Summary

### Common result across shell paths

For both visible serial candidates:

- Path A sent the exact `@CFG:` frame and received `0` bytes
- Path B completed boot, sent the request after boot, and still received `0` bytes
- `has_cfg_frame` remained `false`

This means the failure reproduces outside the browser and outside a single serial interface choice.

### Browser-path result

- WCH explicit browser selection still timed out after:
  - `open_start`
  - `open_end`
  - `signals_deasserted`
  - `reader_start`
- Browser explicit usbmodem selection was not isolated because fresh Incognito windows did not present a chooser option for a different port after the first WCH grant in that Incognito session

## Matrix

| Port candidate | Path | Command used | Open parameters | Bytes sent | Bytes received | `@CFG` frame | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `/dev/cu.wchusbserial5B5E0200881` | A raw wire probe | `python - /dev/cu.wchusbserial5B5E0200881 <<'PY' ...` with exact `config.get_capabilities` envelope | `baud=115200`, `timeout=0.2`, `write_timeout=1`, line signals default | `114` | `0` | `false` | silent failure |
| `/dev/cu.usbmodem5B5E0200881` | A raw wire probe | `python - /dev/cu.usbmodem5B5E0200881 <<'PY' ...` with exact `config.get_capabilities` envelope | `baud=115200`, `timeout=0.2`, `write_timeout=1`, line signals default | `114` | `0` | `false` | silent failure |
| `/dev/cu.wchusbserial5B5E0200881` | B repro tool | `python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --wait-for-boot` | `baud=115200`, `timeout=0.2`, `write_timeout=1`, `setDTR(false)`, `setRTS(false)`, wait-for-boot enabled | `114` | `0` after boot | `false` | boot seen, no reply |
| `/dev/cu.usbmodem5B5E0200881` | B repro tool | `python scripts/repro_cfg_timeout.py --port /dev/cu.usbmodem5B5E0200881 --command get_capabilities --wait-for-boot` | `baud=115200`, `timeout=0.2`, `write_timeout=1`, `setDTR(false)`, `setRTS(false)`, wait-for-boot enabled | `114` | `0` after boot | `false` | boot seen, no reply |
| WCH selected in browser chooser | C webapp path | `Config -> Capabilities` at `http://localhost:3000/USB2BLE`, chooser selection `USB Single Serial (cu.wchusbserial5B5E0200881)` | browser Web Serial open, app logs show `baudRate: 115200`, `signals_deasserted`, reader started | `114` expected frame size, inferred from command envelope | no parsed response frame observed in browser logs | `false` observed | timed out |
| usbmodem explicit browser selection | C webapp path | attempted via fresh Incognito windows | not isolated successfully; chooser did not offer a second candidate after the first session’s grant state | not isolated | not isolated | not isolated | inconclusive |

## Detailed Evidence

### Path A evidence

#### WCH

- File:
  - `evidence/20260407/CFG-TRANSPORT-ISO/path-a-cu-wchusbserial.log`
- Summary:
  - `SUMMARY {"bytes_received": 0, "bytes_sent": 114, "command": "config.get_capabilities", "has_cfg_frame": false, "open_params": {"baudrate": 115200, "signals": "default", "timeout": 0.2, "write_timeout": 1}, "path": "A", "port": "/dev/cu.wchusbserial5B5E0200881", "raw_ascii_preview": "", "raw_hex_preview": ""}`

#### usbmodem

- File:
  - `evidence/20260407/CFG-TRANSPORT-ISO/path-a-cu-usbmodem.log`
- Summary:
  - `SUMMARY {"bytes_received": 0, "bytes_sent": 114, "command": "config.get_capabilities", "has_cfg_frame": false, "open_params": {"baudrate": 115200, "signals": "default", "timeout": 0.2, "write_timeout": 1}, "path": "A", "port": "/dev/cu.usbmodem5B5E0200881", "raw_ascii_preview": "", "raw_hex_preview": ""}`

Interpretation:

- immediate raw sends on both interfaces produced no bytes at all
- no interface-specific success signal appeared

### Path B evidence

#### WCH

- File:
  - `evidence/20260407/CFG-TRANSPORT-ISO/path-b-cu-wchusbserial.log`
- Summary:
  - `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.get_capabilities", "frame_len": 114, "has_cfg_frame": false, "port": "/dev/cu.wchusbserial5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`

#### usbmodem

- File:
  - `evidence/20260407/CFG-TRANSPORT-ISO/path-b-cu-usbmodem.log`
- Summary:
  - `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.get_capabilities", "frame_len": 114, "has_cfg_frame": false, "port": "/dev/cu.usbmodem5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`

Interpretation:

- both interfaces boot
- both interfaces accept post-boot send timing
- neither interface returns any `@CFG:` frame

### Path C evidence

#### WCH explicit browser run

User-observed status:

- `Command config.get_capabilities timed out (ID: 1)`

Relevant browser lifecycle logs:

- `owner_acquire_requested`
- `open_start`
- `open_end`
- `signals_deasserted`
- `reader_start`
- `owner_acquire_succeeded`
- after ~5 seconds: `owner_acquire_failed` with timeout message
- `disconnect_start`
- `reader_done`
- `reader_end`
- `disconnect_end`

Identity shown in browser logs:

- `usbVendorId: 6790`
- `usbProductId: 21971`

Interpretation:

- browser attach completes
- signal deassertion runs
- reader starts
- no response is parsed before timeout

#### usbmodem browser isolation attempt

User-observed result:

- after opening another Incognito window, the chooser did not present a different port candidate even after retries and hard refreshes

Interpretation:

- explicit browser-path usbmodem isolation was not completed
- this is a browser-session limitation, not positive evidence that usbmodem succeeds

## Layer Isolation Readout

### Browser layer

Not primary.

Reason:

- WCH browser flow times out, but both shell paths also fail on both ports
- the failure survives removal of the browser entirely

### Serial interface layer

Not isolated to one interface.

Reason:

- WCH fails in Path A and Path B
- usbmodem fails in Path A and Path B
- both ports show the same high-level silent-response behavior under shell control

### Firmware command runtime layer

Most likely primary failing layer.

Reason:

- a valid `config.get_capabilities` request sent outside the browser on both visible interfaces still produces zero response bytes
- this persists even after:
  - boot completes
  - DTR/RTS are deasserted
  - the request is sent post-boot

### Mixed factors

Secondary, but not primary.

Reason:

- browser chooser/session behavior can make per-port isolation awkward
- interfaces can flip between `busy` and available depending on recent opens
- however, the common denominator remains the firmware-side absence of any `@CFG:` reply

## Recommendation

Recommended failing layer classification:

- `firmware`

Reasoned call:

- If the browser were the primary failure, at least one shell path should have produced a valid `@CFG:` reply.
- If the serial interface were the primary failure, at least one of the two visible candidates should have behaved differently under shell control.
- Instead, both shell paths fail across both visible interfaces with the same silent-result pattern:
  - request sent
  - `0` response bytes
  - no `@CFG:` frame

Best current objective statement:

- the browser shows the symptom
- interface ownership/chooser behavior adds noise
- but the dominant failing layer is the firmware-side config command runtime on the flashed image

## Next Step

The next investigation should target the firmware runtime attach path directly:

- prove whether the config transport runtime task actually starts on the flashed image
- prove whether it consumes any received bytes on the live UART path
- prove whether it ever enqueues or writes a response frame
