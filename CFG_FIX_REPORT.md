# CFG Fix Report

Historical note:

- This report records the narrow web-transport fix attempt and its automated validation at that point in time.
- Later retained hardware evidence showed that this fix alone did not close the full browser config CRUD lane.
- Current retained status is summarized in:
  - `CFG_TRANSPORT_BINDING_AUDIT.md`
  - `CFG_TRANSPORT_ISOLATION_MATRIX_POSTFIX.md`
  - `browser-roundtrip-proof.md`

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`
HEAD: `ce0023b392bb5c388a4ad92804d1075c9ade179e`

## Root Cause Fixed In This Change

The config web transport had two failure-prone behaviors in the exact timeout path:

1. It registered the response waiter only after `writer.write(frame)` completed.
   If the device replied immediately, the read loop could parse the `@CFG:` response before a waiter existed, silently dropping the reply and leaving the command to time out.
2. It opened the serial port without matching the console attach behavior.
   The config path was not deasserting DTR/RTS and did not give the port a short post-open settle window, which made it more likely to send into the ESP32 reboot window caused by USB-UART attach.

This change fixes that web transport race/attach behavior with minimal blast radius.

## Changed Files

- `charm-web-companion/lib/adapters/SerialConfigTransport.ts`
- `charm-web-companion/__tests__/config.test.ts`

## What Changed

### `SerialConfigTransport`

- Registers the command waiter before writing the `@CFG:` frame.
- Cleans up the waiter on timeout and write failure.
- Returns `kTransportFailure` when the transport write itself fails.
- Deasserts `dataTerminalReady` and `requestToSend` after port open.
- Adds a short post-open settle delay before commands are sent.

### Targeted Regression Tests

- Added immediate `get_capabilities` response coverage so a reply arriving during `write()` is no longer dropped.
- Added full `persist` request/response coverage, including exact wire frame verification.
- Added timeout/no-response coverage proving the waiter is cleaned up correctly.

## Test Proof

Commands run successfully after the patch:

1. `cmake --build build/unit --parallel`
   Result: pass
2. `ctest --test-dir build/unit --output-on-failure`
   Result: `20/20` passed
3. `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run __tests__/config.test.ts`
   Result: `15/15` passed
4. `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run`
   Result: `6` files passed, `60/60` tests passed
5. `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npm run build`
   Result: pass

## Residual Risks

- Hardware CFG validation has not been re-run yet, so this change is proven by unit/web tests but not yet by fresh `CFG-01` and `CFG-02` evidence.
- The firmware-side UART runtime startup path in `app_bootstrap.cpp` still has limited observability. If hardware timeout persists after this patch, the next investigation target should remain the live config runtime attach on the primary console UART.
- Config still depends on choosing the actual runtime input interface; the browser transport is now safer, but it does not independently prove the wrong port was never selected by the operator.
