# Charm Web Companion Architecture

## Why the app is static-only
The Charm Web Companion is a pure static site by design. It requires no backend, no authentication, no database, no cloud synchronization, and no remote fleet management. This ensures that the tool is entirely self-contained, privacy-respecting, and can be hosted anywhere (or run locally) without relying on external server infrastructure. All interactions with the device happen directly from the browser via Web APIs.

## Why Web Serial is the primary transport
The current firmware supports flashing, console monitoring, and configuration commands exclusively over a serial connection. While future iterations might explore BLE, the current truth boundary dictates that Web Serial is the only supported transport for these operations. The browser's Web Serial API allows direct, secure communication with the ESP32-S3 without requiring native companion apps or drivers.

## Why serial ownership exists
A serial port can only be reliably controlled by one logical owner at a time. To prevent race conditions, interleaved data, and broken states, the application enforces strict mutual exclusion:
- The owner can be `none`, `flash`, `console`, or `config`.
- There is no simultaneous multi-owner serial access.
- Configuration commands sent to the device are serial commands and must respect these ownership rules (typically requiring `flash` or `console` to yield, or temporarily claiming ownership if the port is free).
- Handoffs between states (e.g., finishing a flash and opening the console) must be explicit and safe.

## Why config transport must reflect firmware truth instead of UI assumptions
The browser-based local draft editor allows users to create rich mapping configurations, but the firmware still owns the runtime-effective bundle format.

The current implemented contract is:
- browser draft authoring stays browser-local
- `config.persist` sends a versioned `mapping_document`
- firmware compiles that document into a `CompiledMappingBundle`
- firmware persists the compiled bundle bytes plus `mapping_bundle` ref, `profile_id`, and optional bonding material

The UI must keep that boundary honest:
- Local drafts, JSON import/export, and browser-local saves are browser-side features.
- Device `persist`/`load`/`clear` operations must describe compiled-bundle behavior truthfully.
- The web UI must not imply raw draft persistence or hidden firmware capabilities that do not exist.

## Profile support contract
- The implemented firmware-backed profile set is exactly two-profile:
  - `Generic BLE Gamepad`
  - `Wireless Xbox Controller`
- The Config UI now exposes those two profile IDs as constrained choices only:
  - `profile_id = 1`
  - `profile_id = 2`
- `config.get_capabilities` does not dynamically enumerate those profiles; the UI treats them as the shipped branch contract.
- Hardware BLE proof is still part of the release gate for either profile claim.

## Validate view boundary
- `Validate` is a browser/Gamepad API surface.
- It does not imply firmware, BLE transport, or internal runtime inspection.
