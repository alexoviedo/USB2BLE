# WEB_PROFILE_UI_MATRIX.md

## Scope
- Web companion Config UX for intentional selection of `profile_id = 1` and `profile_id = 2`
- Serial-first `@CFG:` persist/load/get_capabilities contract
- Honest UI handling for unsupported or rejected profile responses

## Matrix

| Flow | Expected behavior | Observed automated evidence |
| --- | --- | --- |
| Profile selector render | Config view shows exactly two constrained choices: `Generic BLE Gamepad (1)` and `Wireless Xbox Controller (2)` | `charm-web-companion/__tests__/config-view.test.tsx` asserts both labeled radio controls render |
| Persist profile 1 | Persist sends `payload.profile_id = 1` and success status reflects Generic profile | `config-view.test.tsx` asserts request payload and success copy for profile `1` |
| Persist profile 2 | Persist sends `payload.profile_id = 2` and success status reflects Xbox profile | `config-view.test.tsx` asserts request payload and success copy for profile `2` |
| Load persisted profile | Successful `config.load` updates the selected UI profile when device returns a supported `profile_id` | `config-view.test.tsx` asserts load response with `profile_id = 2` reselects Xbox |
| Unsupported persist rejection | UI surfaces a profile-specific error when firmware rejects a selected profile with `kUnsupportedCapability` | `config-view.test.tsx` asserts explicit Xbox rejection message |
| Unsupported loaded profile | UI fails visibly if device returns an out-of-contract `profile_id` instead of silently accepting it | `config-view.test.tsx` asserts unsupported `profile_id=99` error state |
| Schema guardrail | Browser-side persist payload validation accepts `1` and `2`, rejects unsupported IDs | `charm-web-companion/__tests__/config.test.ts` covers `profile_id = 2` success and `99` rejection |
| Capability honesty | `config.get_capabilities` remains serial-contract-only and is not treated as a dynamic profile registry | Config UI copy and `CONFIG_TRANSPORT_CONTRACT.md` explicitly document this boundary |

## Validation Commands
- `cd charm-web-companion && PATH=/Users/alex/.nvm/versions/node/v20.19.4/bin:$PATH npx vitest run __tests__/config.test.ts __tests__/config-view.test.tsx`

## Acceptance Notes
- The UI intentionally exposes only the shipped supported set for this branch.
- No free-form profile entry exists in the browser.
- No UI copy claims that `config.get_capabilities` reports profile IDs.
