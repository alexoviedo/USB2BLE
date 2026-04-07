# PROFILE_OUTPUT_MATRIX.md

## Outcome
The firmware/runtime output layer now supports two explicitly declared profiles:
- `profile_id = 1`: `Generic BLE Gamepad`
- `profile_id = 2`: `Wireless Xbox Controller`

This branch validates profile support at four layers:
- manager selection and capability introspection
- per-profile encoder correctness
- runtime BLE profile selection and notify shape
- config persist/apply carry-through for both supported profile IDs

## Profile Matrix

| Profile ID | Profile Name | Status | Report ID | Report Size | Declared Capabilities | Automated Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| `1` | `Generic BLE Gamepad` | implemented, validated | `1` | `9` bytes | `SupportsHat`, `SupportsAnalogTriggers` | `tests/unit/test_profile_manager.cpp`, `tests/unit/test_profile_generic_gamepad_encoder.cpp`, `tests/unit/test_ble_transport_adapter.cpp`, `tests/unit/test_runtime_data_plane.cpp` |
| `2` | `Wireless Xbox Controller` | implemented, validated | `2` | `13` bytes | `SupportsAnalogTriggers`, `MapsHatToDpadButtons`, `SupportsHighResolutionAxes` | `tests/unit/test_profile_manager.cpp`, `tests/unit/test_profile_wireless_xbox_controller_encoder.cpp`, `tests/unit/test_ble_transport_adapter.cpp`, `tests/unit/test_runtime_data_plane.cpp`, `tests/unit/test_config_transport_service.cpp` |

## Capability and Behavior Deltas

### Generic BLE Gamepad
- Preserves the existing Generic output path.
- Emits the canonical logical hat value directly.
- Encodes four primary axes at `int8` resolution.
- Encodes left and right triggers at `uint8` resolution.
- Encodes the first 16 logical buttons into a two-byte mask.

### Wireless Xbox Controller
- Adds a second encoder without changing the Generic report contract.
- Converts the canonical hat state into D-pad button bits.
- Expands the first four canonical axes into signed `int16` values.
- Preserves `uint8` trigger encoding.
- Encodes the first 16 logical buttons into a `uint16` mask.

## BLE-Facing Verification Criteria

The BLE path is not considered verified just because the encoder emits a different byte shape. This branch now requires all of the following to be true in automated proof:

- `RuntimeDataPlane` selects the active BLE profile explicitly before notify.
- `BleTransportAdapter` records a profile-specific BLE contract:
  - `profile_id`
  - `report_id`
  - `report_size`
  - device-facing name
- `BleTransportAdapter` rejects a notify request whose `report_id` or byte size does not match the active BLE profile contract.
- Switching from Generic to Xbox while the BLE adapter is running forces a bounded adapter restart so the BLE-facing contract is reapplied instead of silently reusing the old Generic presentation.
- Adapter status events now surface:
  - `active_profile`
  - `active_report_id`
  - `active_report_size`
- ESP-side logging hooks now emit BLE profile configuration lines in the form:
  - `ble profile configured profile=<id> name=<name> report_id=<id> bytes=<size>`

## Capability Introspection Contract
- `CanonicalProfileManager::GetSupportedProfiles()` returns both profile descriptors.
- Each descriptor now includes:
  - `profile_id`
  - `name`
  - `report_id`
  - `report_size`
  - `capabilities[]`
- Unsupported profile selection is rejected by the profile manager.
- If runtime state references an unsupported active profile, `RuntimeDataPlane` now skips BLE notify instead of emitting a misleading report.

## Test Evidence

Commands run:
- `cmake --build build/unit --parallel`
- `ctest --test-dir build/unit --output-on-failure`
- `ctest --test-dir build/unit --output-on-failure -R 'BleTransportAdapterTest|RuntimeDataPlaneTest'`

Observed results:
- Full unit suite: `20/20` tests passed.
- Focused BLE/runtime suite: `2/2` tests passed.

Coverage highlights:
- Profile manager:
  - both supported profiles appear in introspection
  - select profile success for `1` and `2`
  - unsupported profile rejection
  - encode-without-selection and request/selection mismatch rejection
- Generic encoder:
  - capability descriptor correctness
  - zero/default report shape
  - button/hat/axis/trigger encoding
  - clamp behavior
- Xbox encoder:
  - capability descriptor correctness
  - HID null-hat to neutral D-pad handling
  - HID directional hat to D-pad mapping
  - button/axis/trigger encoding
  - clamp behavior
- BLE adapter:
  - default Generic BLE contract is configured on start
  - selecting Xbox configures a distinct BLE-facing contract with report ID `2` and size `13`
  - selecting a new profile while running restarts the adapter to reapply the BLE-facing contract
  - mismatched report contract is rejected instead of being sent under the wrong BLE-facing profile
- Runtime data plane:
  - selected Generic profile drives BLE profile selection and Generic report emission
  - selected Xbox profile emits Xbox-shaped BLE output
  - selected Xbox profile drives BLE profile selection before notify
  - BLE profile selection failure suppresses notify
  - unsupported selected profile produces no BLE notify
- Config transport:
  - `config.persist` carries `profile_id = 2` through storage and supervisor activation

## Honest Limits
- The current web companion Config UI now exposes both supported profile IDs, but `config.get_capabilities` still does not dynamically enumerate them.
- Hardware validation for BLE pairing/report behavior remains a separate release gate; this matrix covers automated proof only.
- The canonical logical hat contract is still effectively HID-native in the firmware path, so the Xbox encoder explicitly treats HID null-hat as neutral and maps directional values into D-pad button bits.
