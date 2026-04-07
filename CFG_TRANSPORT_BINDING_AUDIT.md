# CFG Transport Binding Audit

Date: 2026-04-07
Repo: `USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD under audit: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Scope

Goal: prove which runtime backend the firmware uses for config-frame ingress and egress on actual hardware, then compare that binding to the host-visible serial path.

## Root Cause

The repeated `@CFG` timeout incident was not caused by the runtime binding choosing the wrong serial backend in source.

What the audit proved:

- the firmware config runtime is bound to `UART_NUM_0` for both reads and writes
- the current rebuilt and reflashed image starts that runtime successfully on hardware
- the current rebuilt and reflashed image reads a `config.get_capabilities` request, parses it, emits a response, and returns a framed `@CFG` reply to the host

The strongest remaining explanation for the earlier failures is image provenance drift rather than a live transport-binding defect:

- retained failing evidence came from a different flashed image baseline, including logs that reported `ESP-IDF v6.1-dev-3669-gc32c7152ef`
- the audited image that succeeds was rebuilt locally and flashed from the current branch on `ESP-IDF v5.5.3`

In other words, the code binding path was correct; the misleading symptom came from testing against an older or different flashed image than the one represented by the audited tree.

Evidence bundle:

- `evidence/20260407/CFG-TRANSPORT-AUDIT/commands.txt`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/result.json`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083016Z-host-serial-ports.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083019Z-unit-build-runtime-adapter.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083028Z-unit-test-runtime-adapter.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083036Z-idf-build.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083141Z-idf-flash-wch.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083211Z-repro-wch-get-capabilities.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083220Z-repro-usbmodem-get-capabilities.log`
- `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083247Z-host-port-metadata.log`

## Current Binding

Source inspection shows the config transport runtime is hard-bound to `UART_NUM_0` for both directions:

- Incoming config bytes are read via `uart_read_bytes(UART_NUM_0, ...)` in `components/charm_app/src/app_bootstrap.cpp` around line `133`.
- Outgoing `@CFG:` frames are written via `uart_write_bytes(UART_NUM_0, ...)` in `components/charm_app/src/app_bootstrap.cpp` around line `110`.
- The runtime is installed and started from `components/charm_app/src/app_bootstrap.cpp` around lines `177-219`.

Retained observability hooks for this path:

- Compile-time audit guard `CHARM_CFG_TRANSPORT_BINDING_AUDIT` remains in `components/charm_app/src/app_bootstrap.cpp` and now defaults to `0`, so detailed audit logs are opt-in.
- Bootstrap anomaly warnings remain always available in `components/charm_app/src/app_bootstrap.cpp` for `driver_install`, `param_config`, and `task_create` failures.
- Write-path anomaly warnings remain always available in `components/charm_app/src/app_bootstrap.cpp`, but are bounded to one warning per continuous stall streak rather than one warning per loop iteration.
- Read / parse / emit counters remain exposed by `components/charm_app/include/charm/app/config_transport_runtime_adapter.hpp` and `components/charm_app/src/config_transport_runtime_adapter.cpp` so unit tests and intentional audit builds can observe the transport path without leaving verbose logging on by default.

## Expected Host Path

From the firmware binding alone, the expected host entry point is the host-visible serial interface that reaches `UART0` at `115200`.

On this macOS host, the two visible candidates are:

- `/dev/cu.wchusbserial5B5E0200881`
- `/dev/cu.usbmodem5B5E0200881`

The port metadata audit shows they are not two different USB devices. Both resolve to the same USB serial endpoint:

- `VID:PID=1A86:55D3`
- `serial=5B5E020088`
- `location=20-6`
- `product="USB Single Serial"`

See `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083247Z-host-port-metadata.log`.

## Hardware Proof

### Boot and runtime startup

The flashed audit build booted successfully and the runtime came up cleanly:

- `cfg_transport_audit: runtime started port=0 baud=115200`
- `cfg_transport_audit: bootstrap port=0 baud=115200 install_rc=0 param_rc=0 task_rc=1`

Those lines appear in both `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083211Z-repro-wch-get-capabilities.log` and `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083220Z-repro-usbmodem-get-capabilities.log`.

### WCH-hosted repro

When the shell repro targeted `/dev/cu.wchusbserial5B5E0200881`, the runtime logged:

- `rx port=0 bytes_read=114 bytes_total=114 parsed_frames=1 emitted_frames=0`
- `tx port=0 parsed_frames=1 emitted_frames=1`

The host then received a framed reply:

- `@CFG:{... "command":"config.get_capabilities","status":"kOk", ...}`
- summary: `has_cfg_frame: true`, `response_bytes: 496`

See `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083211Z-repro-wch-get-capabilities.log`.

### usbmodem-hosted repro

When the same repro targeted `/dev/cu.usbmodem5B5E0200881`, the runtime logged the same counters:

- `rx port=0 bytes_read=114 bytes_total=114 parsed_frames=1 emitted_frames=0`
- `tx port=0 parsed_frames=1 emitted_frames=1`

The host again received a framed reply:

- `@CFG:{... "command":"config.get_capabilities","status":"kOk", ...}`
- summary: `has_cfg_frame: true`, `response_bytes: 496`

See `evidence/20260407/CFG-TRANSPORT-AUDIT/artifacts/20260407T083220Z-repro-usbmodem-get-capabilities.log`.

## Mismatch Analysis

Current conclusion: there is no live binding mismatch on the audited flashed image.

What is now proven:

- The firmware backend is `UART_NUM_0`.
- The runtime starts successfully on hardware.
- The runtime reads the exact request length, parses one frame, emits one frame, and the host receives the reply.
- Both macOS callout nodes in question reach the same underlying USB serial device rather than two distinct transport backends.

What this weakens:

- A pure browser-path failure as the primary cause.
- A wrong-port choice between `/dev/cu.wchusbserial...` and `/dev/cu.usbmodem...` as the primary cause on this machine.

Most likely historical mismatch:

- Earlier failures were captured on a different flashed image and toolchain baseline:
  - failing logs showed `ESP-IDF v6.1-dev-3669-gc32c7152ef`
  - this audited build is `ESP-IDF v5.5.3`
- After rebuilding and flashing the current tree, the config runtime responds correctly on hardware.

That means the strongest mismatch is between the previously failing flashed image and the current audited image, not between the runtime binding code and the host-visible serial path.

## Fix Summary

The production-safe finalization for this transport path is:

- keep the functional runtime binding unchanged on `UART_NUM_0`
- keep the audit counters in the runtime adapter for tests and targeted audit sessions
- gate verbose startup and per-frame audit logs behind `CHARM_CFG_TRANSPORT_BINDING_AUDIT=1`
- leave only bounded anomaly warnings on by default so production builds stay quiet unless runtime startup or writeout actually fails

No transport rebind was required because the audited hardware run proved the existing `UART_NUM_0` path is correct.

## Retained Final Observability Policy

Default production behavior:

- no per-request `rx` or `tx` audit logs
- no routine startup audit info logs
- warning logs only for bootstrap failures or persistent write stalls

Intentional audit behavior:

- compile with `CHARM_CFG_TRANSPORT_BINDING_AUDIT=1` to re-enable the detailed `runtime started`, `bootstrap ready`, `rx`, and `tx` audit lines used in the binding investigation
- use the retained runtime counters to validate parsed and emitted frame counts in tests or targeted audit builds

## Bottom Line

The config runtime binding is:

- ingress: `UART_NUM_0`
- egress: `UART_NUM_0`
- observed host path on this machine: both `/dev/cu.wchusbserial5B5E0200881` and `/dev/cu.usbmodem5B5E0200881` reach the same live transport and successfully carry `@CFG` traffic after the audited rebuild/flash

If the timeout reappears, the next comparison point should be flashed image provenance and ESP-IDF/toolchain version before revisiting browser transport ownership.
