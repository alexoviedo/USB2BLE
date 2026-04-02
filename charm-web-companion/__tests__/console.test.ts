import { describe, it, expect, vi, beforeEach } from 'vitest';
import { WebSerialMonitor } from '@/lib/adapters/SerialMonitor';

describe('WebSerialMonitor', () => {
  let monitor: WebSerialMonitor;
  let mockPort: any;
  let mockReadableReader: any;
  let mockWritableWriter: any;

  beforeEach(() => {
    monitor = new WebSerialMonitor();

    mockReadableReader = {
      read: vi.fn().mockResolvedValue({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };

    mockWritableWriter = {
      write: vi.fn().mockResolvedValue(undefined),
      releaseLock: vi.fn(),
      close: vi.fn().mockResolvedValue(undefined),
    };

    mockPort = {
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      setSignals: vi.fn().mockResolvedValue(undefined),
      readable: {
        getReader: vi.fn(() => mockReadableReader),
      },
      writable: {
        getWriter: vi.fn(() => mockWritableWriter),
      },
      getInfo: vi.fn(() => ({ usbVendorId: 0x303a, usbProductId: 0x1001 })),
    };

    Object.defineProperty(global.navigator, 'serial', {
      value: {
        getPorts: vi.fn().mockResolvedValue([]),
        requestPort: vi.fn().mockResolvedValue(mockPort),
      },
      configurable: true,
    });

    Object.defineProperty(global.window, 'isSecureContext', {
      value: true,
      configurable: true,
    });
  });

  it('throws if Web Serial is not supported', async () => {
    Object.defineProperty(global.navigator, 'serial', {
      value: undefined,
      configurable: true,
    });
    await expect(monitor.connect()).rejects.toThrow(/Web Serial API not supported/);
  });

  it('throws if not secure context', async () => {
    Object.defineProperty(global.window, 'isSecureContext', {
      value: false,
      configurable: true,
    });
    await expect(monitor.connect()).rejects.toThrow(/Secure context required/);
  });

  it('connects successfully and decodes bytes', async () => {
    const onData = vi.fn();
    monitor.onData(onData);

    mockReadableReader.read
      .mockResolvedValueOnce({ value: new Uint8Array([104, 101, 108, 108, 111]), done: false })
      .mockResolvedValueOnce({ value: undefined, done: true });

    await monitor.connect(115200);

    expect(mockPort.open).toHaveBeenCalledWith({ baudRate: 115200 });
    expect(mockPort.setSignals).toHaveBeenCalledWith({ dataTerminalReady: true, requestToSend: false });
    await new Promise((resolve) => setTimeout(resolve, 0));

    expect(onData).toHaveBeenCalledWith('hello');
    expect(monitor.getDiagnostics().bytesReceived).toBe(5);
  });

  it('prefers an already granted port', async () => {
    const existingPort = { ...mockPort };
    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([existingPort]);

    await monitor.connect();

    expect(global.navigator.serial.requestPort).not.toHaveBeenCalled();
    expect(existingPort.open).toHaveBeenCalled();
  });

  it('throws when readable stream is unavailable', async () => {
    const badPort = {
      ...mockPort,
      readable: null,
      close: vi.fn().mockResolvedValue(undefined),
    };

    global.navigator.serial.requestPort = vi.fn().mockResolvedValue(badPort);

    await expect(monitor.connect()).rejects.toThrow(/no readable stream/i);
    expect(badPort.close).toHaveBeenCalled();
  });

  it('disconnects cleanly', async () => {
    mockReadableReader.read.mockResolvedValueOnce({ value: new Uint8Array([116]), done: false });
    await monitor.connect();
    await new Promise((resolve) => setTimeout(resolve, 0));
    await monitor.disconnect();

    expect(mockPort.close).toHaveBeenCalled();
  });

  it('handles read errors and triggers onError callback', async () => {
    const onError = vi.fn();
    monitor.onError(onError);

    mockReadableReader.read.mockRejectedValue(new Error('Device unplugged'));

    await monitor.connect();
    await new Promise((resolve) => setTimeout(resolve, 0));

    expect(onError).toHaveBeenCalled();
    expect(onError.mock.calls[0][0].message).toContain('Device unplugged');
  });

  it('retries open when runtime port is still re-enumerating', async () => {
    mockPort.open
      .mockRejectedValueOnce(new DOMException('Port unavailable', 'NetworkError'))
      .mockResolvedValueOnce(undefined);

    await monitor.connect(115200);

    expect(mockPort.open).toHaveBeenCalledTimes(2);
  });
});
