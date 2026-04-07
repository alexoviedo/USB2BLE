# CFG-01 Scenario Notes

## Scenario

- ID: `CFG-01`
- Goal: `config.get_capabilities` roundtrip
- Scope: post-fix release-gate rerun

## Session Metadata

- Date: `2026-04-07`
- Repo root: `/Users/alex/Developer/CodexUSB/USB2BLE`
- Branch: `codex/freeze-contract-plan`
- Commit: `ce0023b`

## Host Precheck

- `ctest --test-dir build/unit --output-on-failure`
- Result: `PASS`
- Recorded artifact:
  - `artifacts/20260407T074421Z-host-precheck.log`

## Operator Notes

- Serial port used:
  - Browser chooser in Incognito: `USB Single Serial (cu.wchusbserial5B5E0200881)`
  - Fresh shell repro: `/dev/cu.wchusbserial5B5E0200881`
- Web app URL used:
  - `http://localhost:3000/USB2BLE`
- Device state before test:
  - Fresh local dev server was restarted and verified healthy before the rerun
  - Board boots normally on the visible runtime serial path and shows firmware startup logs for commit `ce0023b`

## Observations

- Step 1:
  - `http://localhost:3000/USB2BLE` loaded correctly
  - Config controls were visible:
    - `Capabilities`
    - `Load Current`
    - `Persist to Device`
- Step 2:
  - Normal browser `Capabilities` attempt did not show a chooser and timed out with:
    - `Command config.get_capabilities timed out (ID: 1)`
  - Incognito retry showed the chooser
  - Operator explicitly selected:
    - `USB Single Serial (cu.wchusbserial5B5E0200881)`
  - Incognito retry produced the same timeout:
    - `Command config.get_capabilities timed out (ID: 1)`
- Step 3:
  - Fresh post-fix shell repro artifact:
    - `artifacts/20260407T075255Z-post-fix-shell-repro-get-capabilities.log`
  - The post-fix repro completed boot and then sent `config.get_capabilities`
  - Captured stream includes:
    - `rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)`
    - `Project name:     charm`
    - `App version:      ce0023b`
    - `charm_usb_host: USB host stack ready`
    - `BOOT_PHASE_DONE True`
    - `REQUEST_SENT`
    - `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.get_capabilities", "frame_len": 114, "has_cfg_frame": false, "port": "/dev/cu.wchusbserial5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`
  - Retained `serial.log` was refreshed from the post-fix repro
  - No `@CFG:` response was observed before timeout

## Outcome

- Final result:
  - `FAIL`
- Notes:
  - `config.get_capabilities` still cannot be proven on hardware after the web transport fix
  - Per protocol, execution stops here and returns to the T2/T3 loop
  - This scenario is retained as current post-fix failure evidence, not release-passing proof
