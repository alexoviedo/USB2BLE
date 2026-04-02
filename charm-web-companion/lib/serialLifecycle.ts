export type SerialLifecycleOwner = 'flash' | 'console' | 'config' | 'store';

export interface SerialPortIdentity {
  usbVendorId?: number;
  usbProductId?: number;
  serialNumber?: string;
  usbProductName?: string;
}

const PREFERRED_RUNTIME_PORT_KEY = 'charm_preferred_runtime_port';
const LAST_FLASH_PORT_KEY = 'charm_last_flash_port';

export function logSerialLifecycleEvent(
  owner: SerialLifecycleOwner,
  event: string,
  details: Record<string, unknown> = {}
): void {
  try {
    const payload = {
      at: new Date().toISOString(),
      owner,
      event,
      ...details,
    };
    console.info('[serial:lifecycle]', payload);
  } catch {
    // no-op for logging failures
  }
}

export function getSerialPortIdentity(port: SerialPort | null): SerialPortIdentity | null {
  if (!port) return null;

  try {
    const info = typeof port.getInfo === 'function' ? port.getInfo() : {};
    const maybePort = port as any;
    return {
      usbVendorId: info?.usbVendorId,
      usbProductId: info?.usbProductId,
      serialNumber:
        typeof maybePort?.serialNumber === 'string' && maybePort.serialNumber.length > 0
          ? maybePort.serialNumber
          : undefined,
      usbProductName:
        typeof maybePort?.usbProductName === 'string' && maybePort.usbProductName.length > 0
          ? maybePort.usbProductName
          : undefined,
    };
  } catch {
    return null;
  }
}

export function portIdentityKey(identity: SerialPortIdentity | null): string {
  if (!identity) return 'unknown';
  return [
    identity.usbVendorId ?? 'na',
    identity.usbProductId ?? 'na',
    identity.serialNumber ?? 'na',
    identity.usbProductName ?? 'na',
  ].join(':');
}

export function sameSerialPortIdentity(a: SerialPortIdentity | null, b: SerialPortIdentity | null): boolean {
  if (!a || !b) return false;
  if (a.serialNumber && b.serialNumber) {
    return a.serialNumber === b.serialNumber;
  }
  return (
    a.usbVendorId === b.usbVendorId &&
    a.usbProductId === b.usbProductId &&
    (a.usbProductName ?? '') === (b.usbProductName ?? '')
  );
}

export function savePreferredRuntimePort(identity: SerialPortIdentity | null): void {
  if (!identity) return;
  try {
    window.localStorage.setItem(PREFERRED_RUNTIME_PORT_KEY, JSON.stringify(identity));
  } catch {
    // ignore storage issues
  }
}

export function loadPreferredRuntimePort(): SerialPortIdentity | null {
  return loadFromStorage(PREFERRED_RUNTIME_PORT_KEY);
}

export function saveLastFlashPort(identity: SerialPortIdentity | null): void {
  if (!identity) return;
  try {
    window.localStorage.setItem(
      LAST_FLASH_PORT_KEY,
      JSON.stringify({
        identity,
        flashedAt: Date.now(),
      })
    );
  } catch {
    // ignore storage issues
  }
}

export function loadLastFlashPort(): { identity: SerialPortIdentity; flashedAt: number } | null {
  return loadFromStorage(LAST_FLASH_PORT_KEY);
}

function loadFromStorage<T>(key: string): T | null {
  try {
    const raw = window.localStorage.getItem(key);
    if (!raw) return null;
    return JSON.parse(raw) as T;
  } catch {
    return null;
  }
}
