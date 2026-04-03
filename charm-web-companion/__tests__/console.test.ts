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

    global.window.localStorage.clear();
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

    mockReadableReader.read
      .mockResolvedValueOnce({ value: new Uint8Array([79, 75]), done: false })
      .mockRejectedValueOnce(new Error('Device unplugged'));

    await monitor.connect();
    await new Promise((resolve) => setTimeout(resolve, 0));

    expect(onError).toHaveBeenCalled();
    expect(onError.mock.calls[0][0].message).toContain('Device unplugged');
  });

  it('retries open when runtime port is still re-enumerating', async () => {
    mockPort.open
      .mockRejectedValueOnce(new DOMException('Port unavailable', 'NetworkError'))
      .mockResolvedValueOnce(undefined);
    mockReadableReader.read
      .mockResolvedValueOnce({ value: new Uint8Array([65]), done: false })
      .mockResolvedValueOnce({ value: undefined, done: true });

    await monitor.connect(115200);

    expect(mockPort.open).toHaveBeenCalledTimes(2);
  });

  it('handles stale granted stream by falling back to requestPort', async () => {
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
      getInfo: vi.fn(() => ({ usbVendorId: 0x1111, usbProductId: 0x2222 })),
    };

    const requestedReader = {
      read: vi.fn()
        .mockResolvedValueOnce({ value: new Uint8Array([65]), done: false })
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

    expect(global.navigator.serial.requestPort).toHaveBeenCalled();
    expect(requestedPort.open).toHaveBeenCalled();
    expect(stalePort.close).toHaveBeenCalled();
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
    mockReadableReader.read
      .mockResolvedValueOnce({ value: new Uint8Array([79, 75]), done: false })
      .mockResolvedValueOnce({ value: undefined, done: true });

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

  it('rejects quiet auto-selected port and surfaces inactive stream', async () => {
    vi.useFakeTimers();
    let releaseQuietRead: (() => void) | null = null;
    const quietReader = {
      read: vi.fn(() => new Promise((resolve) => {
        releaseQuietRead = () => resolve({ value: undefined, done: true });
      })),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockImplementation(async () => {
        releaseQuietRead?.();
      }),
    };
    const quietPort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => quietReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x2222, usbProductId: 0x4444 })),
    };
    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([quietPort]);
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(new DOMException('cancel', 'NotFoundError'));

    try {
      const connectPromise = monitor.connect().catch((error) => {
        expect(error).toMatchObject({ code: 'STREAM_INACTIVE' });
        return null;
      });
      await vi.advanceTimersByTimeAsync(4100);
      await connectPromise;
      expect(quietPort.open).toHaveBeenCalled();
    } finally {
      vi.useRealTimers();
    }
  });


  it('accepts a quiet stream when user explicitly selects the port', async () => {
    vi.useFakeTimers();
    let releaseQuietRead: (() => void) | null = null;
    const quietReader = {
      read: vi.fn(() => new Promise((resolve) => {
        releaseQuietRead = () => resolve({ value: undefined, done: true });
      })),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockImplementation(async () => {
        releaseQuietRead?.();
      }),
    };
    const requestedQuietPort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => quietReader) },
      getInfo: vi.fn(() => ({ usbVendorId: 0x7777, usbProductId: 0x8888 })),
    };

    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([]);
    global.navigator.serial.requestPort = vi.fn().mockResolvedValue(requestedQuietPort);

    try {
      const connectPromise = monitor.connect();
      await vi.advanceTimersByTimeAsync(4100);
      await connectPromise;

      expect(requestedQuietPort.open).toHaveBeenCalled();
      expect(monitor.getDiagnostics().isReading).toBe(true);

      await monitor.disconnect();
    } finally {
      vi.useRealTimers();
    }
  });

  it('accepts a quiet granted port when it matches the preferred runtime port outside the flash window', async () => {
    vi.useFakeTimers();
    let releaseQuietRead: (() => void) | null = null;
    const quietReader = {
      read: vi.fn(() => new Promise((resolve) => {
        releaseQuietRead = () => resolve({ value: undefined, done: true });
      })),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockImplementation(async () => {
        releaseQuietRead?.();
      }),
    };
    const quietIdentity = { usbVendorId: 0x1a86, usbProductId: 0x55d3 };
    const quietPort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => quietReader) },
      getInfo: vi.fn(() => quietIdentity),
    };

    global.window.localStorage.setItem('charm_preferred_runtime_port', JSON.stringify(quietIdentity));
    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([quietPort]);
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(new DOMException('cancel', 'NotFoundError'));

    try {
      const connectPromise = monitor.connect();
      await vi.advanceTimersByTimeAsync(4100);
      await connectPromise;

      expect(quietPort.open).toHaveBeenCalled();
      expect(global.navigator.serial.requestPort).not.toHaveBeenCalled();
      expect(monitor.getDiagnostics().isReading).toBe(true);

      await monitor.disconnect();
    } finally {
      vi.useRealTimers();
    }
  });

  it('still rejects a quiet preferred granted port during the recent flash window', async () => {
    vi.useFakeTimers();
    let releaseQuietRead: (() => void) | null = null;
    const quietReader = {
      read: vi.fn(() => new Promise((resolve) => {
        releaseQuietRead = () => resolve({ value: undefined, done: true });
      })),
      releaseLock: vi.fn(),
      cancel: vi.fn().mockImplementation(async () => {
        releaseQuietRead?.();
      }),
    };
    const quietIdentity = { usbVendorId: 0x1a86, usbProductId: 0x55d3 };
    const quietPort = {
      ...mockPort,
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: { getReader: vi.fn(() => quietReader) },
      getInfo: vi.fn(() => quietIdentity),
    };

    global.window.localStorage.setItem('charm_preferred_runtime_port', JSON.stringify(quietIdentity));
    global.window.localStorage.setItem(
      'charm_last_flash_port',
      JSON.stringify({ identity: quietIdentity, flashedAt: Date.now() })
    );
    global.navigator.serial.getPorts = vi.fn().mockResolvedValue([quietPort]);
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(new DOMException('cancel', 'NotFoundError'));

    try {
      const connectPromise = monitor.connect().catch((error) => {
        expect(error).toMatchObject({ code: 'STREAM_INACTIVE' });
        return null;
      });
      await vi.advanceTimersByTimeAsync(6000);
      await connectPromise;

      expect(quietPort.open).toHaveBeenCalled();
      expect(global.navigator.serial.requestPort).toHaveBeenCalled();
    } finally {
      vi.useRealTimers();
    }
  }, 10000);

  it('supports repeated flash-to-console style connect/disconnect cycles', async () => {
    vi.useRealTimers();
    for (let i = 0; i < 3; i++) {
      const cycleReader = {
        read: vi.fn()
          .mockResolvedValueOnce({ value: new Uint8Array([65 + i]), done: false })
          .mockResolvedValueOnce({ value: undefined, done: true }),
        releaseLock: vi.fn(),
        cancel: vi.fn().mockResolvedValue(undefined),
      };
      const cyclePort = {
        ...mockPort,
        open: vi.fn().mockResolvedValue(undefined),
        close: vi.fn().mockResolvedValue(undefined),
        readable: { getReader: vi.fn(() => cycleReader) },
      };
      global.navigator.serial.getPorts = vi.fn().mockResolvedValue([]);
      global.navigator.serial.requestPort = vi.fn().mockResolvedValue(cyclePort);

      await monitor.connect();
      await monitor.disconnect();

      expect(cyclePort.open).toHaveBeenCalledTimes(1);
      expect(cyclePort.close).toHaveBeenCalledTimes(1);
    }
  });

});
