import { describe, it, expect, vi, beforeEach } from 'vitest';
import { WebSerialMonitor } from '@/lib/adapters/SerialMonitor';

describe('WebSerialMonitor', () => {
  let monitor: WebSerialMonitor;
  let mockPort: any;
  let mockReadable: any;
  let mockWritable: any;
  let mockReader: any;
  let mockWriter: any;

  beforeEach(() => {
    monitor = new WebSerialMonitor();

    mockReader = {
      read: vi.fn().mockResolvedValue({ value: 'test log\n', done: false }),
      releaseLock: vi.fn(),
      cancel: vi.fn()
    };

    mockWriter = {
      write: vi.fn().mockResolvedValue(undefined),
      releaseLock: vi.fn(),
      close: vi.fn()
    };

    mockReadable = {
      pipeTo: vi.fn().mockResolvedValue(undefined)
    };

    mockWritable = {};

    mockPort = {
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      readable: mockReadable,
      writable: mockWritable
    };

    // Mock TextDecoderStream and TextEncoderStream
    global.TextDecoderStream = class {
      readable = { getReader: () => mockReader };
      writable = {};
    } as any;

    global.TextEncoderStream = class {
      readable = { pipeTo: vi.fn() };
      writable = { getWriter: () => mockWriter };
    } as any;

    // Mock navigator.serial
    Object.defineProperty(global.navigator, 'serial', {
      value: { requestPort: vi.fn().mockResolvedValue(mockPort) },
      configurable: true
    });

    // Mock secure context
    Object.defineProperty(global.window, 'isSecureContext', {
      value: true,
      configurable: true
    });
  });

  it('throws if Web Serial is not supported', async () => {
    Object.defineProperty(global.navigator, 'serial', { value: undefined, configurable: true });
    await expect(monitor.connect()).rejects.toThrow(/Web Serial API not supported/);
  });

  it('throws if not secure context', async () => {
    Object.defineProperty(global.window, 'isSecureContext', { value: false, configurable: true });
    await expect(monitor.connect()).rejects.toThrow(/Secure context required/);
  });

  it('connects successfully and starts reading', async () => {
    const onData = vi.fn();
    monitor.onData(onData);

    // We need to make the reader eventually return done: true to avoid infinite loop in test
    mockReader.read
      .mockResolvedValueOnce({ value: 'hello', done: false })
      .mockResolvedValueOnce({ value: undefined, done: true });

    await monitor.connect(115200);

    expect(mockPort.open).toHaveBeenCalledWith({ baudRate: 115200 });
    
    // Wait a tick for the async reading loop to process
    await new Promise(resolve => setTimeout(resolve, 0));
    
    expect(onData).toHaveBeenCalledWith('hello');
  });

  it('handles port busy / open failure', async () => {
    mockPort.open.mockRejectedValue(new Error('Failed to open serial port'));
    await expect(monitor.connect()).rejects.toThrow(/Serial port is busy or unavailable/);
  });

  it('handles permission denied', async () => {
    const error = new Error('SecurityError');
    error.name = 'SecurityError';
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(error);
    
    await expect(monitor.connect()).rejects.toThrow(/Permission denied/);
  });

  it('handles user cancellation', async () => {
    const error = new Error('NotFoundError');
    error.name = 'NotFoundError';
    global.navigator.serial.requestPort = vi.fn().mockRejectedValue(error);
    
    await expect(monitor.connect()).rejects.toThrow(/No port selected/);
  });

  it('disconnects cleanly', async () => {
    // Keep reader active so it doesn't auto-close before we disconnect
    mockReader.read.mockResolvedValueOnce({ value: 'test', done: false });
    await monitor.connect();
    // Wait for startReading to initialize reader
    await new Promise(resolve => setTimeout(resolve, 0));
    await monitor.disconnect();

    expect(mockReader.cancel).toHaveBeenCalled();
    expect(mockPort.close).toHaveBeenCalled();
  });

  it('handles read errors and triggers onError callback', async () => {
    const onError = vi.fn();
    monitor.onError(onError);

    mockReader.read.mockRejectedValue(new Error('Device unplugged'));

    await monitor.connect();
    await new Promise(resolve => setTimeout(resolve, 0));

    expect(onError).toHaveBeenCalled();
    expect(onError.mock.calls[0][0].message).toContain('Device unplugged');
  });
});
