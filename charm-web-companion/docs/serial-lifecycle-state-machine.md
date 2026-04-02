# Serial Lifecycle State Machine (Flash / Console / Config)

```mermaid
stateDiagram-v2
  [*] --> idle
  idle --> flash_owned: FlashView startFlash
  idle --> console_owned: ConsoleView connect
  idle --> config_owned: ConfigView runDeviceCommand

  flash_owned --> flash_connected: flasher.connect() open_start/open_end
  flash_connected --> flash_flashing: write_flash
  flash_flashing --> flash_resetting: hard_reset
  flash_resetting --> idle: flasher.disconnect() disconnect_end + owner none

  console_owned --> console_attaching: monitor.connect candidate open_start/open_end
  console_attaching --> console_streaming: reader_start + data received
  console_attaching --> idle: STALE_PORT / PORT_BUSY / PORT_DISCONNECTED
  console_streaming --> idle: manual disconnect / stream end / inactivity

  config_owned --> config_connected: transport.connect open_start/open_end
  config_connected --> config_command: sendCommand
  config_command --> idle: transport.disconnect disconnect_end + owner none

  flash_owned --> flash_owned: ownership lock prevents console/config attach
  console_owned --> console_owned: ownership lock prevents flash/config attach
  config_owned --> config_owned: ownership lock prevents flash/console attach
```

## Key invariants
- Only one owner can hold serial at a time (`none|flash|console|config`).
- Every error path attempts deterministic unwind (`reader.cancel`/`releaseLock`/`close`).
- Structured logs are emitted for owner changes, open/close, reader start/end, and disconnect reasons.
