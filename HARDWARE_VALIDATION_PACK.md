# HARDWARE_VALIDATION_PACK.md

## Purpose
Concrete execution pack for physical validation that automated testing cannot prove.

## Release Gate Rule

No release candidate for the active production lane is shippable until every required scenario below has retained evidence.

That includes the current implemented firmware/web companion lane whenever a release claims:

- compiler-backed config/apply
- `Generic BLE Gamepad`
- `Wireless Xbox Controller`

## Evidence Root
Store all artifacts under:
`evidence/<YYYYMMDD>/<scenario-id>/`

Each scenario should include:
- `commands.txt` (exact commands executed)
- `serial.log` (raw monitor output)
- `result.json` (`PASS`/`FAIL` + notes)

Recommended helper:
- `scripts/hardware_evidence.sh` scaffolds scenario directories and keeps `commands.txt` readable by writing command stdout/stderr to per-step logs under `artifacts/`

## Hardware Matrix
- HW-A: primary ESP32-S3 board + USB cable
- HW-B: second ESP32-S3 board (rollback/regression lane)
- HW-C: BLE peer device (phone or host BLE tool)
- HUB-1: powered USB hub for multi-device tests

## Scenario Matrix

| ID | Scenario | Required | Pass criteria |
|---|---|---|---|
| CFG-01 | `config.get_capabilities` roundtrip | Yes | Response is `@CFG:` framed and `status=kOk` |
| CFG-02 | Current `v2` `config.persist` / `config.load` / `config.clear` metadata roundtrip | Yes | Expected status + persisted compiled-bundle metadata/profile data returned over serial framing |
| CFG-03 | Compiler-backed config apply over serial-first transport | Yes | Profile-selected compile/apply flow succeeds on-device and survives reboot/reconnect |
| USB-01 | USB host enumeration with one HID device | Yes | Device/interface/report events appear and path reaches BLE notify readiness logs |
| USB-02 | USB host with powered hub + 2 devices | Yes | Deterministic behavior under dual devices, no crash/fault loop |
| BLE-01 | `Generic BLE Gamepad` connect + report channel ready + notify | Yes | Generic profile pairs, stays connected, and produces live input reports |
| BLE-02 | `Wireless Xbox Controller` connect + report channel ready + notify | Yes | Xbox profile pairs, stays connected, and produces live input reports |
| BLE-03 | BLE disconnect/reconnect recovery for the active profile | Yes | Recover or fail closed deterministically; no runaway loop |
| OPS-01 | Flash -> reboot -> console attach on a real ESP32-S3 | Yes | Flash completes, device reboots, console attaches, and logs are visible |
| STG-01 | Cold boot storage init and persisted activation path | Yes | Startup logs show storage init success path and expected persisted state activation |
| REL-01 | Candidate artifact checksum/provenance verification | Yes | `sha256sum -c` passes; provenance fields present |
| REL-02 | Rollback rehearsal on HW-B | Yes | Prior known-good release restored and verified |

## Commands

### Evidence helper
```bash
scripts/hardware_evidence.sh bootstrap <date>
scripts/hardware_evidence.sh init <date> <scenario>
scripts/hardware_evidence.sh record <date> <scenario> host-precheck -- cmake --build build/unit --parallel
```

The helper keeps `commands.txt` focused on:
- exact command line
- output log path under `artifacts/`
- exit code

That avoids multi-gigabyte transcript files while still retaining the command trace.

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
- Profile-aware config/UI evidence showing selected `profile_id = 1` or `profile_id = 2` where applicable
- BLE lifecycle/recovery events from firmware logs (adapter status transitions)
- USB/device enumeration and report-processing anchors from firmware logs
- Flash/console handoff anchors showing successful reset and serial log visibility

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
- Any FAIL in mandatory CFG/USB/BLE/OPS/STG/REL scenarios.
- Missing evidence artifacts for mandatory scenarios.
- Integrity verification failure.
