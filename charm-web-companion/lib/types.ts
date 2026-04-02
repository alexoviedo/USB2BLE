export type SerialOwner = 'none' | 'flash' | 'console' | 'config';

export type SerialPermissionState =
  | 'unknown'
  | 'requesting_permission'
  | 'permission_granted'
  | 'permission_denied'
  | 'request_cancelled'
  | 'port_busy'
  | 'unsupported';

export class AppError extends Error {
  constructor(public message: string, public code: string) {
    super(message);
    this.name = 'AppError';
  }
}

export class SerialError extends AppError {
  constructor(message: string, code: string = 'SERIAL_ERROR') {
    super(message, code);
    this.name = 'SerialError';
  }
}

export class FlashError extends AppError {
  constructor(message: string, code: string = 'FLASH_ERROR') {
    super(message, code);
    this.name = 'FlashError';
  }
}

export class ConfigError extends AppError {
  constructor(message: string, code: string = 'CONFIG_ERROR') {
    super(message, code);
    this.name = 'ConfigError';
  }
}
