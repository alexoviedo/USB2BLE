# TODO.md

## Status legend
- queued
- in_progress
- done
- blocked

## Firmware backlog

| Order | Item | Status | Notes |
|---|---|---|---|
| 1 | Execute full physical hardware matrix (`HARDWARE_VALIDATION_PACK.md`) | queued | Requires boards, BLE peers, and powered USB-hub coverage |
| 2 | Capture release-candidate firmware evidence | queued | After hardware matrix completion |
| 3 | Rehearse firmware artifact rollback on a second board | queued | Use prior known-good artifact set |

## Open blockers
- Physical hardware evidence is still pending; host CI does not substitute for device validation.
