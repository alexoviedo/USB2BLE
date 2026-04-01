# CONFIG_TRANSPORT_CONTRACT.md

## Purpose
Define the production serial-first host/device configuration transport contract and framing used by the firmware runtime.

## Status
- Contract version: `v1`
- Runtime status: implemented in the firmware parser/runtime adapter

## Chosen First Transport Path
1. **First transport path:** line-framed request/response messages over the existing device serial channel.
2. **Serial primary?:** Yes. Serial is the only primary transport for first-production config write/persist.
3. **BLE config transport:** Deferred (future path only after separate proof/contract work).

## Command Set (v1)
- `config.persist`
- `config.load`
- `config.clear`
- `config.get_capabilities`

## Request Shape
- `protocol_version` (required)
- `request_id` (required, unique per in-flight request)
- `command` (required)
- `payload` (command-specific)
- `integrity` (required metadata for validation)

## Wire Framing
- **Frame prefix:** `@CFG:`
- **Wire request format:** `@CFG:{json}\n`
- **Wire response format:** `@CFG:{json}\n`
- Frames without the `@CFG:` prefix are ignored by the firmware config runtime transport so human-readable logs can coexist on the same serial stream without contaminating config parsing.

## Response Shape
- `protocol_version` (required)
- `request_id` (required; must match request)
- `status` (required; maps to firmware `ContractStatus`)
- `fault` (optional; required when status is non-ok; maps to firmware `FaultCode`)
- `payload` (command-specific output)

## Error Mapping
- Transport framing and I/O faults remain transport-level errors and must not be rewritten as firmware-domain success.
- Firmware returns preserve existing contract semantics:
  - `kOk`
  - `kRejected`
  - `kUnavailable`
  - `kFailed`

## Persistence Expectations
- `config.persist` writes mapping bundle/profile payload to the firmware config-store boundary only.
- Persisted write is explicit.
- `config.load` is read-only and returns the current persisted view.
- `config.clear` removes persisted config and returns explicit status/fault.
- Bonding material in the first path is an optional payload field and may be absent.

## Versioning + Integrity Expectations
- Contract versioning is explicit via `protocol_version` and must be validated before command execution.
- Message integrity metadata is required in request/response envelopes for verification.
- Unsupported versions must fail closed with explicit non-ok status/fault and no state mutation.

## Failure + Rollback Behavior
- Fail closed on malformed envelope, unsupported version, or integrity mismatch.
- If persist fails, the previous persisted configuration remains authoritative.
- No speculative success path should be assumed by tooling until explicit firmware response status is received.

## Explicit Non-Support in First Slice
- No BLE config transport in the first production path.
- No multi-transport negotiation or racing.
- No streaming or partial config-upload semantics.
- No background auto-retry persist loops.
- No schema migration/version-upgrade logic beyond strict version acceptance/rejection.
