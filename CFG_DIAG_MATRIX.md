# CFG Diagnosis Matrix

Historical note:

- This document freezes the mid-incident diagnosis state from earlier on April 7, 2026.
- It is not the latest branch-level conclusion for transport binding or config status.
- Later retained evidence supersedes the binding-layer conclusion here:
  - `CFG_TRANSPORT_BINDING_AUDIT.md`
  - `CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md`
  - `browser-roundtrip-proof.md`

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Executive Summary

The current incident is not just a browser timeout. The strongest live evidence is that the retained runtime UART port (`/dev/cu.wchusbserial5B5E0200881`) still produces zero `@CFG:` response bytes even when:

- DTR/RTS are explicitly deasserted
- the board is allowed to finish booting first
- the request is sent from a direct serial repro script instead of the web app

That shifts the highest-likelihood root cause away from request framing and toward the live firmware transport attach/startup path on the device-facing UART. The browser/webapp layer still has real issues, but they look like compounding factors rather than the only cause.

## Executed Repros

### Live shell repros run during diagnosis

1. `python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --wait-for-boot`
   Result: `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.get_capabilities", "frame_len": 114, "has_cfg_frame": false, "port": "/dev/cu.wchusbserial5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`
2. `python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command persist --wait-for-boot`
   Result: `SUMMARY {"boot_bytes": 3221, "boot_complete": true, "command": "config.persist", "frame_len": 356, "has_cfg_frame": false, "port": "/dev/cu.wchusbserial5B5E0200881", "request_id": 9401, "response_bytes": 0, "signals_deasserted": true, "wait_for_boot": true}`

### Retained incident evidence already on disk

- `evidence/20260407/CFG-01/serial.log`
- `evidence/20260407/CFG-01/artifacts/20260407T070806Z-shell-probe-get-capabilities-usbmodem.log`
- `evidence/20260407/CFG-01/artifacts/20260407T072500Z-delayed-send-get-capabilities.log`
- `evidence/20260407/CFG-01/result.json`
- `evidence/20260407/CFG-02/serial.log`
- `evidence/20260407/CFG-02/artifacts/20260407T071301Z-shell-probe-persist.log`
- `evidence/20260407/CFG-02/result.json`
- `CFG_INCIDENT_REPORT.md`

## Diagnosis Matrix

| Layer | Check | Observation | Evidence | Likelihood |
| --- | --- | --- | --- | --- |
| A) Browser/webapp | Request payload and framing correctness | Wire format is correct in tests. `persist` request shape is asserted exactly, and live repro frame sizes are 114 and 356 bytes, both far below the 2048-byte cap. | `charm-web-companion/__tests__/config.test.ts:132-158`, `scripts/repro_cfg_timeout.py`, live repro summaries | Low |
| A) Browser/webapp | Serial ownership conflicts | `ConfigView` refuses to run if another owner holds serial. The failures occurred after config acquired ownership, so contention is not the best fit. | `charm-web-companion/components/views/ConfigView.tsx:253-309` | Low |
| A) Browser/webapp | Chosen port identity vs expected runtime interface | Config transport reuses granted ports and picks by identity only. It does not verify active runtime traffic the way the console path does. This matters because the board exposes both `/dev/cu.usbmodem...` and `/dev/cu.wchusbserial...`. | `charm-web-companion/lib/adapters/SerialConfigTransport.ts:35-69`, `charm-web-companion/lib/adapters/SerialMonitor.ts:155-210`, `sdkconfig:1297-1306` | High contributor |
| A) Browser/webapp | Timeout and retry behavior | Config transport writes immediately after open, uses a fixed 5 second timeout, and has no retry or post-open settle window. | `charm-web-companion/lib/adapters/SerialConfigTransport.ts:153-172` | Medium contributor |
| B) Transport/runtime adapter | Parser entry and line framing behavior | Unit tests cover valid frames, chunk boundaries, mixed log streams, malformed frames, and oversized frames. In every parser/service case, the adapter emits some deterministic response when invoked. | `tests/unit/test_config_transport_runtime_adapter.cpp:60-174` | Low |
| B) Transport/runtime adapter | `@CFG:` prefix handling | Runtime adapter ignores non-`@CFG:` log lines and processes prefixed frames correctly in tests. | `components/charm_app/src/config_transport_runtime_adapter.cpp:68-80`, `tests/unit/test_config_transport_runtime_adapter.cpp:149-174` | Low |
| B) Transport/runtime adapter | `request_id` correlation and waiter lifecycle | Browser transport registers the waiter after `writer.write(frame)`, which is a real race if the device replies extremely quickly. But the direct serial repro bypasses this path and still gets zero response bytes. | `charm-web-companion/lib/adapters/SerialConfigTransport.ts:160-171`, live repro summaries | Low to medium |
| B) Transport/runtime adapter | Malformed or oversized frames | Not supported by evidence. `get_capabilities` and `persist` repro frames are small and structurally valid; parser tests already reject malformed frames with explicit `@CFG:` responses, not silence. | `scripts/repro_cfg_timeout.py`, `tests/unit/test_config_transport_runtime_adapter.cpp:78-147` | Low |
| C) Firmware command path | `get_capabilities` handling | Service path deterministically returns `kOk` with capability fields. If the request reaches the service, silence is not expected. | `components/charm_app/src/config_transport_service.cpp:111-120` | Low |
| C) Firmware command path | `persist` preconditions | Invalid persist payloads reject with a response rather than dropping the frame. Silence is not explained by normal validation failure. | `components/charm_app/src/config_transport_service.cpp:36-89` | Low |
| C) Firmware command path | Response frame emission path | The runtime adapter always serializes a pending response after `HandleFrame`. If requests were being consumed, some `@CFG:` frame should appear. | `components/charm_app/src/config_transport_runtime_adapter.cpp:78-97`, `:200-262` | High |
| C) Firmware command path | Fail-closed startup path that logs but never replies | `StartConfigTransportRuntime()` hard-binds to `UART_NUM_0` and ignores return values from `uart_driver_install`, `uart_param_config`, and `xTaskCreatePinnedToCore`. A startup failure here would leave normal app logs visible while config transport stays inert. | `components/charm_app/src/app_bootstrap.cpp:68-131` | High |
| D) Physical serial path | Correct USB interface used | `sdkconfig` sets primary console to UART0 and secondary console to USB Serial/JTAG. Local ESP-IDF docs say the secondary USB Serial/JTAG path is suitable for output, but input requires selecting it as the primary console. That makes `/dev/cu.usbmodem...` a poor candidate for config requests on this firmware. | `sdkconfig:1297-1306`, `/Users/alex/esp/esp-idf/docs/en/api-guides/usb-serial-jtag-console.rst:48-55` | High contributor |
| D) Physical serial path | DTR/RTS impact | Opening the port reboots the board in both browser and shell evidence. The console path explicitly deasserts DTR/RTS; the config path does not. But even after deasserting signals and waiting for boot, direct repro still gets no reply, so this is not the whole root cause. | `charm-web-companion/lib/adapters/SerialMonitor.ts:532-544`, `scripts/repro_cfg_timeout.py:128-145`, live repro summaries | Medium contributor |
| D) Physical serial path | Reboot and race windows after open | Real and reproducible. However, the delayed-send repro after `Returned from app_main()` still yields `response_bytes: 0`, so a pure timing race is insufficient to explain the incident. | `evidence/20260407/CFG-01/artifacts/20260407T072500Z-delayed-send-get-capabilities.log`, live repro summaries | Medium, not primary |

## Hypotheses Ranked By Likelihood

1. The firmware config transport runtime is not actually operational on the live UART0 request path.
   Evidence: both direct serial repros on `/dev/cu.wchusbserial5B5E0200881` show `boot_complete: true` and `response_bytes: 0`; service and adapter unit tests show valid requests should always produce an `@CFG:` reply if consumed at all.
   Why this ranks first: it explains both `config.get_capabilities` and `config.persist`, and it still reproduces after removing browser timing and signal variables.

2. `StartConfigTransportRuntime()` can fail silently on UART0, leaving logs visible but config transport inert.
   Evidence: runtime startup in `app_bootstrap.cpp` discards all return values from `uart_driver_install`, `uart_param_config`, and `xTaskCreatePinnedToCore`; local ESP-IDF UART source explicitly logs `UART driver already installed` and returns failure if install is attempted twice.
   Why this ranks second: it is the cleanest fail-closed explanation inside the currently visible firmware code, though we do not yet have on-device logs proving which startup call failed.

3. The browser config path is prone to choosing the wrong interface and sending too early.
   Evidence: config transport lacks active-port validation, signal stabilization, and initial-activity wait; `sdkconfig` exposes a secondary USB Serial/JTAG output path that local ESP-IDF docs warn is output-oriented unless selected as the primary console.
   Why this is not first: it explains browser timeouts and confusing chooser behavior, but it does not explain the post-boot zero-response direct serial repro on the WCH UART bridge.

4. Browser-side waiter timing has a response-before-waiter race.
   Evidence: the waiter is installed after `writer.write(frame)`.
   Why this ranks lower: the direct serial repro proves the live device emits no response bytes at all, so fixing the waiter alone would not clear the incident.

5. Request framing or payload semantics are wrong.
   Evidence against: tests assert exact framing; valid `get_capabilities` service behavior is simple; `persist` frame is only 356 bytes; malformed/oversized runtime tests produce explicit `@CFG:` rejections, not silence.
   Why this ranks last: the code and evidence both argue against it.

## Top Suspected Root Causes

### 1. Inert firmware-side config transport on UART0

Most likely current root cause: the config runtime task is either never starting correctly, never successfully attaching a driver to UART0, or never receiving bytes from the live request interface. The code path most consistent with the incident is:

- logs keep working, so the board appears healthy
- valid `@CFG:` requests never reach `ConfigTransportService`
- no serialized response is ever emitted

Primary supporting evidence:

- direct post-boot repro on `/dev/cu.wchusbserial5B5E0200881` returns zero response bytes for both commands
- retained `CFG-01` and `CFG-02` `serial.log` files contain only boot logs
- runtime/service unit tests show that once invoked, the parser and service reply deterministically

### 2. Webapp config transport opens the wrong or unstable interface and immediately writes into a reboot window

Most likely secondary contributor: the browser path makes a fragile choice even when the firmware-side issue is fixed.

Primary supporting evidence:

- `SerialConfigTransport.connect()` has no `setSignals({ dataTerminalReady: false, requestToSend: false })`
- it does not wait for initial activity or runtime-port confirmation
- `sdkconfig` enables primary UART0 console plus secondary USB Serial/JTAG output, so the visible `usbmodem` port is not a reliable request path for config input on this build

## Minimum Repro Case

Use the serial-side repro script. It removes browser state, chooser ambiguity, and request framing uncertainty.

### Commands

1. `ls /dev/cu.* | rg 'wchusbserial|usbmodem'`
2. `python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --wait-for-boot`
3. `python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command persist --wait-for-boot`

### Expected current failure signature

- boot log appears immediately after port open
- `BOOT_PHASE_DONE True`
- `REQUEST_SENT`
- `RESPONSE_PHASE_DONE`
- summary reports `has_cfg_frame: false` and `response_bytes: 0`

### Browser repro that matches the retained incident

1. Open `http://localhost:3000/USB2BLE`
2. Go to `Config`
3. Click `Capabilities`
4. If chooser appears, select `USB Single Serial (cu.wchusbserial5B5E0200881)`
5. Observe `Command config.get_capabilities timed out (ID: 1)`

## Current Incident Status

Incident remains open.

Release remains blocked because `CFG-01` and `CFG-02` still fail and the evidence now points to a real runtime transport defect, not just an operator-flow issue.
