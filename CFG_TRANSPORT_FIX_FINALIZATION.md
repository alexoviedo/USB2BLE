# CFG Transport Fix Finalization

Date: 2026-04-07
Repo: `/Users/alex/Developer/CodexUSB/USB2BLE`
Branch: `codex/freeze-contract-plan`

## Root Cause

The transport-binding code was not selecting the wrong backend. The runtime path was already wired correctly to `UART_NUM_0` for both config-frame ingress and egress.

The incident pattern came from testing against a different flashed image baseline than the current audited tree. Earlier retained failures showed an `ESP-IDF v6.1-dev-3669-gc32c7152ef` image, while the audited rebuild and reflash from this branch on `ESP-IDF v5.5.3` responded correctly on hardware and returned framed `@CFG` replies.

## Finalized Fix

The production-safe change is observability hardening, not a transport rebind:

- `CHARM_CFG_TRANSPORT_BINDING_AUDIT` now defaults to `0` in [app_bootstrap.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/components/charm_app/src/app_bootstrap.cpp), so verbose startup and per-frame audit logs are opt-in.
- Runtime bootstrap now emits default-on warnings only for real anomalies:
  - UART driver install failure
  - UART param-config failure
  - runtime task creation failure
- Write-path anomaly logging remains available by default but is bounded to one warning per continuous stall streak, avoiding noisy loop spam.
- Runtime parse / emit counters remain in [config_transport_runtime_adapter.hpp](/Users/alex/Developer/CodexUSB/USB2BLE/components/charm_app/include/charm/app/config_transport_runtime_adapter.hpp) and [config_transport_runtime_adapter.cpp](/Users/alex/Developer/CodexUSB/USB2BLE/components/charm_app/src/config_transport_runtime_adapter.cpp) for targeted tests and audit builds.

## Verification

These checks passed after the finalization cleanup:

- `cmake --build build/unit --target test_config_transport_runtime_adapter --parallel`
- `ctest --test-dir build/unit --output-on-failure -R ConfigTransportRuntimeAdapterTest`
- `source /Users/alex/esp/esp-idf/export.sh && idf.py build`

The binding audit document was updated with the final root-cause and observability policy in [CFG_TRANSPORT_BINDING_AUDIT.md](/Users/alex/Developer/CodexUSB/USB2BLE/CFG_TRANSPORT_BINDING_AUDIT.md).

## Final Observability Policy

Default production builds:

- quiet on successful startup
- quiet on normal request/response traffic
- warn only on bootstrap or write anomalies

Intentional audit builds:

- compile with `CHARM_CFG_TRANSPORT_BINDING_AUDIT=1` to restore the detailed audit lines used during the hardware investigation
