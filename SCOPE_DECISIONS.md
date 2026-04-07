# Scope Decisions

This document records the rationale for the scope changes requested after the first planning pass.

## Decision Table

| Decision | Previous plan | New decision | Rationale | Consequence for implementation |
|---|---|---|---|---|
| Output profile count | Single-profile lane centered on the existing generic profile | Ship exactly two production profiles: `Generic BLE Gamepad` and `Wireless Xbox Controller` | The target product outcome now requires interoperability beyond the original single-profile proof lane. Freezing the count at two prevents uncontrolled profile sprawl while still meeting the desired product outcome. | Profile manager, encoders, BLE transport assumptions, docs, and hardware validation must all be expanded to cover two named profiles. |
| Config compiler scope | Deferred to a later lane | Bring compiler into the first implementation lane | Without a real compiler, the product cannot honestly claim authored config -> runtime behavior. The earlier plan left a major functional gap between browser authoring and runtime activation. | We need a production compiler implementation, diagnostics contract, fixtures, docs, and a real path from compiled output into runtime/apply semantics. |
| Config transport carrier | Serial-first `@CFG` only | Keep serial-first `@CFG` as the required carrier | The repository already has a working serial config path, the web companion is built around Web Serial, and there is no current proof that a different carrier is required. | Implementation should start by preserving the current carrier and framing. If payload needs grow, the allowed change is a documented, versioned expansion under the same serial framing, not an ad hoc transport switch. |
| UI truth boundary | Honest but narrow UI | Keep honest UI boundary even as scope expands | Expanding scope is not a license to advertise fake device capabilities. The app must still distinguish browser-local draft features from true firmware-backed compile/apply behavior. | Web UX and docs must evolve only after firmware/compiler/transport support exists. |
| Device raw draft persistence | Implicitly out of scope in earlier plan | Remains out of scope | Bringing the compiler into scope does not require the firmware to persist arbitrary raw JSON drafts. That would be a separate capability expansion with different storage, migration, and UX implications. | The plan should implement real compile/apply semantics without pretending the device stores browser drafts unless that capability is explicitly added later. |

## Decision Notes

### Why the second profile is frozen at exactly one addition

The amended target asks for `Wireless Xbox Controller`, not an open-ended profile catalog. Freezing the lane at two profiles keeps the implementation bounded and lets us demand symmetrical proof for both profiles instead of half-finishing several.

### Why compiler-in-scope changes the plan materially

The compiler is not just another helper utility. It changes:

- what the mapping layer is responsible for
- what config/apply has to carry
- what the webapp can honestly claim
- what hardware evidence must prove

That is why the implementation plan now treats compiler work as a core milestone rather than deferred cleanup.

### Why serial-first stays, even with broader config/apply goals

Serial already exists in firmware, web, and operator workflows. Keeping it avoids inventing a second control path before the first one is production-complete. The only acceptable evolution during implementation is a smallest-possible, versioned expansion that remains under `@CFG:` and is documented before code lands.

### Why the UI honesty rule is repeated

The easiest way to get a misleading product is to let the UI run ahead of firmware reality. Repeating this constraint is intentional: compiler-backed flows, profile naming, and validation surfaces are only valid if the underlying code and transport actually support them.
