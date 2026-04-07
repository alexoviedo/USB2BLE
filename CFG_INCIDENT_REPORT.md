# CFG_INCIDENT_REPORT.md

Historical note:

- This report freezes the original CFG incident state before later reflashing and postfix transport proof.
- It remains accurate as an incident snapshot, not as the latest branch-level status.
- Current retained status is captured in:
  - `CFG_TRANSPORT_BINDING_AUDIT.md`
  - `CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md`
  - `browser-roundtrip-proof.md`

## Purpose

Freeze the current `CFG-01` / `CFG-02` incident state so reproduction is deterministic and future debugging starts from the same evidence-backed baseline.

## Incident Scope

- Repo root: `/Users/alex/Developer/CodexUSB/USB2BLE`
- Branch: `codex/freeze-contract-plan`
- Firmware commit under test: `ce0023b`
- Incident date: `2026-04-07`
- Affected scenarios:
  - `CFG-01`
  - `CFG-02`

## Current Incident Summary

### `CFG-01` timeout behavior

- Browser flow reached the `Config` view successfully at:
  - `http://localhost:3000/USB2BLE`
- `Capabilities` timed out with:
  - `Command config.get_capabilities timed out (ID: 1)`
- The issue reproduced:
  - in the normal browser session
  - again in Incognito
  - again after explicitly selecting `USB Single Serial (cu.wchusbserial5B5E0200881)` in the chooser
- No passing `@CFG:` roundtrip was observed.

### `CFG-02` timeout behavior

- Browser flow reached the same `Config` view successfully.
- `Persist to Device` timed out with:
  - `Command config.persist timed out (ID: 2)`
- On the recorded failing attempt, the chooser did not appear.
- The required `persist -> load -> clear -> load` sequence never progressed past the first persist operation.

### Shared low-level finding

- Reproducible shell probes against both visible serial interfaces:
  - `/dev/cu.wchusbserial5B5E0200881`
  - `/dev/cu.usbmodem5B5E0200881`
  produced normal reboot / boot logs but no `@CFG:` response frame.
- The retained raw streams consistently show:
  - `rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)`
  - firmware boot log lines for project `charm`
  - `charm_usb_host: USB host stack ready`
- The retained streams do **not** show:
  - any `@CFG:` response line
  - any successful `config.get_capabilities`
  - any successful `config.persist`

Current frozen incident statement:
- opening the visible serial interfaces during CFG transport attempts yields boot output but no retained `@CFG:` response frame before the transport timeout window expires

## Evidence References

### `CFG-01`

- Scenario root:
  - `evidence/20260407/CFG-01/`
- Primary artifacts:
  - `evidence/20260407/CFG-01/commands.txt`
  - `evidence/20260407/CFG-01/serial.log`
  - `evidence/20260407/CFG-01/result.json`
  - `evidence/20260407/CFG-01/scenario-notes.md`
- Supporting artifacts:
  - `evidence/20260407/CFG-01/artifacts/20260407T063916Z-host-precheck.log`
  - `evidence/20260407/CFG-01/artifacts/20260407T070734Z-shell-probe-get-capabilities.log`
  - `evidence/20260407/CFG-01/artifacts/20260407T070745Z-shell-probe-get-capabilities-rerun.log`
  - `evidence/20260407/CFG-01/artifacts/20260407T070806Z-shell-probe-get-capabilities-usbmodem.log`
  - `evidence/20260407/CFG-01/artifacts/20260407T070905Z-capture-serial-log-get-capabilities.log`

### `CFG-02`

- Scenario root:
  - `evidence/20260407/CFG-02/`
- Primary artifacts:
  - `evidence/20260407/CFG-02/commands.txt`
  - `evidence/20260407/CFG-02/serial.log`
  - `evidence/20260407/CFG-02/result.json`
  - `evidence/20260407/CFG-02/scenario-notes.md`
- Supporting artifacts:
  - `evidence/20260407/CFG-02/artifacts/20260407T071301Z-shell-probe-persist.log`

## Reproducibility Checklist

Use this checklist exactly before any new diagnosis step.

### Browser prerequisites

- Use desktop Chromium.
- Use the local dev URL with the configured base path:
  - `http://localhost:3000/USB2BLE`
- Do **not** use:
  - `http://localhost:3000/`
  because that returns a 404 under the current `basePath` config.

### Exact browser flow

#### Reproduce `CFG-01`

1. Start the web companion locally.
2. Open `http://localhost:3000/USB2BLE`.
3. Navigate to `Config`.
4. Ensure the `Generic BLE Gamepad` profile remains selected.
5. Click `Capabilities`.
6. If a chooser appears, explicitly select:
   - `USB Single Serial (cu.wchusbserial5B5E0200881)`
7. Wait for the operation to settle.
8. Record the exact `Operation Status` text.

Expected current incident reproduction:
- red error:
  - `Command config.get_capabilities timed out (ID: 1)`

#### Reproduce `CFG-02`

1. From the same `Config` view, keep the default draft unchanged.
2. Click `Persist to Device`.
3. If a chooser appears, explicitly select:
   - `USB Single Serial (cu.wchusbserial5B5E0200881)`
4. Wait for the operation to settle.
5. Record the exact `Operation Status` text.
6. Stop after the first persist failure.

Expected current incident reproduction:
- red error:
  - `Command config.persist timed out (ID: 2)`

### Exact serial port selection

Freeze these names exactly as the primary tested targets:

- Browser runtime-choice target:
  - `USB Single Serial (cu.wchusbserial5B5E0200881)`
- Additional visible interface observed in shell probes:
  - `/dev/cu.usbmodem5B5E0200881`

Important frozen finding:
- both visible interfaces produced boot logs but no retained `@CFG:` response frame during shell probes

### Exact command sequence

Use the following command sequence to recreate the same context and low-level evidence path.

#### Baseline host check

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE
ctest --test-dir build/unit --output-on-failure
```

#### Start the web app

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE/charm-web-companion
PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run dev
```

#### `CFG-01` shell reproduction

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE
python - <<'PY'
import sys
import time
import serial
port = '/dev/cu.wchusbserial5B5E0200881'
req = '@CFG:{"protocol_version":2,"request_id":9001,"command":"config.get_capabilities","payload":{},"integrity":"CFG1"}\n'
ser = serial.Serial(port=port, baudrate=115200, timeout=0.25, write_timeout=1)
try:
    ser.setDTR(False)
    ser.setRTS(False)
except Exception:
    pass
ser.reset_input_buffer()
ser.write(req.encode())
ser.flush()
end = time.time() + 6.0
while time.time() < end:
    chunk = ser.read(4096)
    if chunk:
        sys.stdout.buffer.write(chunk)
        sys.stdout.flush()
ser.close()
PY
```

#### `CFG-02` shell reproduction

```bash
cd /Users/alex/Developer/CodexUSB/USB2BLE
python - <<'PY'
import sys
import time
import serial
port = '/dev/cu.wchusbserial5B5E0200881'
mapping_document = '{"version":1,"global":{"scale":1,"deadzone":0.08,"clamp_min":-1,"clamp_max":1},"axes":[{"target":"move_x","source_index":0,"scale":1,"deadzone":0.08,"invert":false}],"buttons":[{"target":"action_a","source_index":0}]}'
req = '@CFG:{"protocol_version":2,"request_id":9201,"command":"config.persist","payload":{"mapping_document":' + mapping_document + ',"profile_id":1},"integrity":"CFG1"}\n'
ser = serial.Serial(port=port, baudrate=115200, timeout=0.25, write_timeout=1)
try:
    ser.setDTR(False)
    ser.setRTS(False)
except Exception:
    pass
ser.reset_input_buffer()
ser.write(req.encode())
ser.flush()
end = time.time() + 6.0
while time.time() < end:
    chunk = ser.read(4096)
    if chunk:
        sys.stdout.buffer.write(chunk)
        sys.stdout.flush()
ser.close()
PY
```

Expected current shell reproduction for both commands:
- boot output appears
- no retained `@CFG:` response frame appears

## Release-Gate Status

This incident remains open.

It blocks release closure because:

- `CURRENT_TASK.md` defines retained hardware evidence as the active release blocker
- `HARDWARE_VALIDATION_PACK.md` marks both `CFG-01` and `CFG-02` as required scenarios
- `HARDWARE_VALIDATION_PACK.md` stop-ship rules treat any mandatory CFG failure as a release blocker
- `UPDATED_MILESTONE_STATUS.md` leaves milestone `M6` partial until mandatory retained evidence is collected successfully

Current frozen blocker statement:
- `CFG-01` = `FAIL`
- `CFG-02` = `FAIL`
- incident remains open
- release gate remains blocked until the config transport failure is diagnosed and the scenarios are rerun to successful retained evidence

## Guardrail

Do not broaden into implementation changes from this file alone.

The next phase should be:
1. preserve this reproduction exactly
2. diagnose why serial open/transport attempts yield boot logs without an `@CFG:` response
3. rerun `CFG-01` and `CFG-02` only after a narrow, testable fix hypothesis exists
