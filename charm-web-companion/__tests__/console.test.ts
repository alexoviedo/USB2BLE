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
    const activeReader = {
      read: vi.fn()
        .mockResolvedValueOnce({ value: new Uint8Array([65]), done: false })
        .mockResolvedValueOnce({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };
    const existingPort = { ...mockPort, readable: { getReader: vi.fn(() => activeReader) } };
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

  it('handles stale ports by falling back to requestPort', async () => {
    const stalePort = {
      ...mockPort,
      getInfo: vi.fn().mockReturnValue(null), // simulate ghost port with no info
    };

    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([stalePort]);

    await monitor.connect();

    expect(global.navigator.serial.requestPort).toHaveBeenCalled();
    expect(mockPort.open).toHaveBeenCalled();
  });

  it('fails over to another granted port when initial stream is inactive', async () => {
    const inactiveReader = {
      read: vi.fn().mockResolvedValue({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };
    const activeReader = {
      read: vi.fn()
        .mockResolvedValueOnce({ value: new Uint8Array([111, 107]), done: false })
        .mockResolvedValueOnce({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };

    const inactivePort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => inactiveReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x1111, usbProductId: 0x2222 })),
    };
    const activePort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => activeReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x3333, usbProductId: 0x4444 })),
    };
    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([inactivePort, activePort]);

    const onData = vi.fn();
    monitor.onData(onData);

    await monitor.connect();
    await new Promise((resolve) => setTimeout(resolve, 0));

    expect(inactivePort.open).toHaveBeenCalled();
    expect(inactivePort.close).toHaveBeenCalled();
    expect(activePort.open).toHaveBeenCalled();
    expect(onData).toHaveBeenCalledWith('ok');
  });

  it('surfaces an error when stream ends unexpectedly', async () => {
    const onError = vi.fn();
    monitor.onError(onError);
    mockReadableReader.read.mockResolvedValueOnce({ value: undefined, done: true });

    await monitor.connect();
    await new Promise((resolve) => setTimeout(resolve, 0));

    expect(onError).toHaveBeenCalled();
    expect(onError.mock.calls[0][0].message).toMatch(/stream ended unexpectedly/i);
  });

  it('falls back to requestPort after all granted candidates fail', async () => {
    const stalePort = {
      ...mockPort,
      open: vi.fn().mockRejectedValue(new DOMException('stale', 'InvalidStateError')),
      getInfo: vi.fn(() => ({ usbVendorId: 0x1111, usbProductId: 0x2222 })),
    };
    const requestedReader = {
      read: vi.fn()
        .mockResolvedValueOnce({ value: new Uint8Array([111, 107]), done: false })
        .mockResolvedValueOnce({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };
    const requestedPort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => requestedReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x3333, usbProductId: 0x4444 })),
    };

    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([stalePort]);
    global.navigator.serial.requestPort = vi.fn().mockResolvedValue(requestedPort);

    await monitor.connect();

    expect(stalePort.open).toHaveBeenCalled();
    expect(global.navigator.serial.requestPort).toHaveBeenCalled();
    expect(requestedPort.open).toHaveBeenCalled();

    await monitor.disconnect();
  });

  it('reports stale stream when granted port immediately closes', async () => {
    const staleReader = {
      read: vi.fn().mockResolvedValue({ value: undefined, done: true }),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockResolvedValue(undefined),
    };
    const stalePort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => staleReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x2222, usbProductId: 0x3333 })),
    };

    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([stalePort]);
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(new DOMException('cancel', 'NotFoundError'));

    await expect(monitor.connect()).rejects.toMatchObject({ code: 'STALE_PORT' });
  });

});
