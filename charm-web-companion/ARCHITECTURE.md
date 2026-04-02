# Charm Web Companion Architecture

## Why the app is static-only
The Charm Web Companion is a pure static site by design. It requires no backend, no authentication, no database, no cloud synchronization, and no remote fleet management. This ensures that the tool is entirely self-contained, privacy-respecting, and can be hosted anywhere (or run locally) without relying on external server infrastructure. All interactions with the device happen directly from the browser via Web APIs.

## Why Web Serial is the primary transport
The current firmware supports flashing, console monitoring, and configuration commands exclusively over a serial connection. While future iterations might explore BLE, the current truth boundary dictates that Web Serial is the only supported transport for these operations. The browser's Web Serial API allows direct, secure communication with the ESP32-S3 without requiring native companion apps or drivers.

## Why serial ownership exists
A serial port can only be reliably controlled by one logical owner at a time. To prevent race conditions, interleaved data, and broken states, the application enforces strict mutual exclusion:
- The owner can be `none`, `flash`, or `console`.
- There is no simultaneous flash and console access.
- Configuration commands sent to the device are serial commands and must respect these ownership rules (typically requiring `flash` or `console` to yield, or temporarily claiming ownership if the port is free).
- Handoffs between states (e.g., finishing a flash and opening the console) must be explicit and safe.

## Why config transport must reflect firmware truth instead of UI assumptions
The browser-based local draft editor allows users to create rich, complex mapping configurations. However, the firmware's current `persist`/`load`/`clear` contract is NOT a full arbitrary config upload pipeline. 

The firmware only persists:
- `mapping_bundle` ref (`bundle_id`, `version`, `integrity`)
- `profile_id`
- optional `bonding_material` byte array

It does **NOT** store the full rich mapping draft object over the config transport. Therefore, the UI must honestly reflect this boundary:
- Local drafts, JSON import/export, and browser-local saves are purely browser-side features.
- Device `persist`/`load`/`clear` operations must strictly adhere to the firmware's actual contract. We do not pretend the device is storing the full local draft, avoiding the mistake of hashing the local draft and stuffing it into the `mapping_bundle` fields to simulate full-config storage.
