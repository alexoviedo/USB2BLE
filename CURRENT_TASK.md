# CURRENT_TASK.md

## Active Task
- ID: FW-MAINT-001
- Title: Firmware-focused repository cleanup and CI hardening
- Status: in_progress

## Why this is the active task
The firmware codepaths are the current source of truth. Repository maintenance should preserve firmware behavior, keep host-side validation explicit, and keep CI centered on firmware outcomes.

## Exit criteria
- Firmware codepaths remain unchanged by non-firmware cleanup work.
- GitHub Actions continues to build the firmware successfully.
- Host-side test/build instructions remain explicit and reproducible.
- Webapp-specific code and deployment pipeline are removed from the active repository.
