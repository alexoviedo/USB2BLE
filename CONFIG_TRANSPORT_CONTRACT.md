# CONFIG_TRANSPORT_CONTRACT.md

## Purpose
Define the production serial-first host/device configuration transport contract and the runtime semantics behind it.

## Status
- Current implemented contract version: `v2`
- Current implemented framing: serial-first `@CFG:{json}\n`
- Current implemented compiler path: browser draft -> `mapping_document` -> firmware compile -> persisted compiled bundle -> runtime activation

## Chosen Transport Path
1. First transport path is the existing device serial channel.
2. Serial remains the only production config transport.
3. BLE config transport remains out of scope.

## Implemented Command Set
- `config.persist`
- `config.load`
- `config.clear`
- `config.get_capabilities`

## Request Envelope
- `protocol_version` (required)
- `request_id` (required)
- `command` (required)
- `payload` (command-specific)
- `integrity` (required)

## Response Envelope
- `protocol_version` (required)
- `request_id` (required; matches request)
- `command` (required)
- `status` (required)
- `fault` (always serialized by the runtime adapter; zeroed for success)
- `payload` (command-specific when present)
- `capabilities` (`config.get_capabilities` only)

## Wire Framing
- Frame prefix: `@CFG:`
- Request format: `@CFG:{json}\n`
- Response format: `@CFG:{json}\n`
- Non-`@CFG:` lines are ignored by the firmware runtime adapter so ordinary console logs can share the same serial stream.

## `config.persist` Request Payload
- `mapping_document` (required object)
- `profile_id` (required; firmware/runtime activation in this branch is validated for `1 = Generic BLE Gamepad` and `2 = Wireless Xbox Controller`)
- `bonding_material` (optional byte array)

### `mapping_document` Shape
```json
{
  "version": 1,
  "global": {
    "scale": 1.0,
    "deadzone": 0.08,
    "clamp_min": -1.0,
    "clamp_max": 1.0
  },
  "axes": [
    {
      "target": "move_x",
      "source_index": 0,
      "scale": 1.0,
      "deadzone": 0.08,
      "invert": false
    }
  ],
  "buttons": [
    {
      "target": "action_a",
      "source_index": 0
    }
  ]
}
```

## `config.persist` Semantics
- Firmware validates `protocol_version`, `request_id`, and `integrity` before mutation.
- Firmware compiles `mapping_document` into a `CompiledMappingBundle`.
- `mapping_bundle.integrity` is derived from `ComputeMappingBundleHash(compiled_bundle)`.
- `mapping_bundle.bundle_id` is set from the compiled bundle integrity value (falling back to `1` only if the hash is zero).
- Firmware persists:
  - `mapping_bundle` ref
  - compiled bundle bytes
  - `profile_id`
  - optional `bonding_material`
- Firmware activates the compiled bundle in the runtime loader immediately on successful persist.
- Firmware does not persist the raw browser draft or the raw `mapping_document`.

## `config.load` Response Payload
- `mapping_bundle`
- `profile_id`

Current wire behavior does not return the persisted compiled bundle bytes or raw draft content to the browser.

## `config.clear` Semantics
- Clears persisted config-store state.
- Clears the active compiled bundle loader state.
- Clears supervisor-visible stored mapping/profile refs.

## `config.get_capabilities` Semantics
- Reports protocol version `2`
- Reports support for `persist`, `load`, `clear`, and `get_capabilities`
- Reports `supports_ble_transport = false`

## Integrity + Failure Behavior
- Unsupported versions fail closed.
- Malformed envelopes fail closed.
- Persist compilation failure or store failure leaves the previous persisted compiled bundle authoritative.
- The `integrity` field is required metadata for contract verification. The current request sentinel remains `CFG1`; compiled bundle integrity is tracked separately inside the persisted bundle metadata.

## Explicit Non-Support
- No BLE config transport.
- No background config sync loops.
- No raw draft persistence on-device.
- No fake profile selection beyond what firmware can actually encode.
- No partial or streaming config upload protocol.
- The current web companion Config UI exposes exactly two constrained profile choices: `1 = Generic BLE Gamepad` and `2 = Wireless Xbox Controller`.
- `config.get_capabilities` still does not dynamically enumerate profiles; the web UI must treat the supported profile set as the shipped branch contract, not a runtime-discovered capability list.
