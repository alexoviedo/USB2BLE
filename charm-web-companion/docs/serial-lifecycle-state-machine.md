# Serial Lifecycle State Machine (Flash / Console / Config)

```mermaid
stateDiagram-v2
  [*] --> idle
  idle --> flash_owner_acquire: FlashView startFlash
  idle --> console_owner_acquire: ConsoleView connect
  idle --> config_owner_acquire: ConfigView runDeviceCommand

  flash_owner_acquire --> flash_connected: owner_acquire_succeeded + open_end
  flash_connected --> flash_flashing: write_flash
  flash_flashing --> flash_resetting: hard_reset
  flash_resetting --> flash_released: disconnect_end
  flash_released --> idle: owner_release_succeeded

  console_owner_acquire --> console_candidate_select: granted_candidates_scored + candidate_selected
  console_candidate_select --> console_opened: open_end
  console_opened --> console_viable: reader_start + initial activity bytes > 0
  console_opened --> console_candidate_select: initial_activity_timeout on granted candidate => STREAM_INACTIVE
  console_opened --> console_quiet_user_selected: initial_activity_timeout on requested candidate
  console_quiet_user_selected --> console_viable: first byte arrives
  console_viable --> idle: manual disconnect / owner_release_succeeded
  console_viable --> idle: reader_done|reader_error => STREAM_INACTIVE

  config_owner_acquire --> config_connected: open_end + reader_start
  config_connected --> config_command: sendCommand
  config_command --> idle: disconnect_end + owner_release_succeeded

  flash_owner_acquire --> flash_owner_acquire: ownership lock prevents console/config attach
  console_owner_acquire --> console_owner_acquire: ownership lock prevents flash/config attach
  config_owner_acquire --> config_owner_acquire: ownership lock prevents flash/console attach
```

## Key invariants
- Only one owner can hold serial at a time (`none|flash|console|config`).
- Console "Connected" now requires stream viability (`reader_start`) and candidate qualification, not `port.open()` alone.
- Auto-selected granted ports that remain inactive are treated as `STREAM_INACTIVE` candidates and rejected.
- Requested ports can remain connected-but-quiet to support firmware that intentionally emits no logs.
- Every error path attempts deterministic unwind (`reader.cancel`/`releaseLock`/`close`) and emits lifecycle logs with port identity.
