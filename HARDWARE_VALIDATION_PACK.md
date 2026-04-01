# HARDWARE_VALIDATION_PACK.md

## Purpose
Concrete execution pack for physical validation that host CI cannot prove.

## Evidence Root
Store all artifacts under:
`evidence/<YYYYMMDD>/<scenario-id>/`

Each scenario should include:
- `commands.txt` (exact commands executed)
- `serial.log` (raw monitor output)
- `result.json` (`PASS`/`FAIL` + notes)

## Hardware Matrix
- HW-A: primary ESP32-S3 board + USB cable
- HW-B: second ESP32-S3 board (rollback/regression lane)
- HW-C: BLE peer device (phone or host BLE tool)
- HUB-1: powered USB hub for multi-device tests

## Scenario Matrix

| ID | Scenario | Required | Pass criteria |
|---|---|---|---|
| CFG-01 | `config.get_capabilities` roundtrip | Yes | Response is `@CFG:` framed and `status=kOk` |
| CFG-02 | `config.load` / `config.persist` / `config.clear` | Yes | Expected status + data returned over serial framing |
| USB-01 | USB host enumeration with one HID device | Yes | Device/interface/report events appear and path reaches BLE notify readiness logs |
| USB-02 | USB host with powered hub + 2 devices | Yes | Deterministic behavior under dual devices, no crash/fault loop |
| BLE-01 | BLE connect + report channel ready + notify | Yes | Notify path operational with stable connection |
| BLE-02 | BLE disconnect/reconnect recovery | Yes | Recover or fail closed deterministically; no runaway loop |
| STG-01 | Cold boot storage init path | Yes | Startup logs show storage init success path |
| REL-01 | Candidate artifact checksum/provenance verification | Yes | `sha256sum -c` passes; provenance fields present |
| REL-02 | Rollback rehearsal on HW-B | Yes | Prior known-good release restored and verified |

## Commands

### Build + artifacts
```bash
idf.py set-target esp32s3
idf.py build
```

### Host-side pre-check
```bash
cmake -S tests/unit -B build/unit
cmake --build build/unit --parallel
ctest --test-dir build/unit --output-on-failure
```

### Integrity verification
```bash
cd <artifact_dir>
sha256sum -c SHA256SUMS
cat provenance.json
```

### Serial capture example
```bash
python -m serial.tools.miniterm /dev/ttyUSB0 115200 | tee evidence/<date>/<scenario>/serial.log
```

## Expected Log Anchors
- Config frames: lines beginning with `@CFG:`
- BLE lifecycle/recovery events from firmware logs (adapter status transitions)
- USB/device enumeration and report-processing anchors from firmware logs

## Result Recording Template
```json
{
  "scenario": "USB-02",
  "date_utc": "2026-03-31T00:00:00Z",
  "result": "PASS",
  "hardware": ["HW-A", "HUB-1"],
  "firmware_commit": "<sha>",
  "notes": "short factual summary",
  "evidence": [
    "commands.txt",
    "serial.log"
  ]
}
```

## Stop-ship Conditions
- Any FAIL in CFG/USB/BLE mandatory scenarios.
- Missing evidence artifacts for mandatory scenarios.
- Integrity verification failure.
