import { SerialMonitorAdapter } from './types';

export interface SerialMonitorDiagnostics {
  bytesReceived: number;
  chunksReceived: number;
  isReading: boolean;
  portInfo: { usbVendorId?: number; usbProductId?: number } | null;
  lastDataAtMs: number | null;
}

const PREFERRED_PORT_INFO_KEY = 'charm_preferred_port_info';
const PORT_OPEN_RETRY_DELAYS_MS = [150, 350, 700] as const;
const INITIAL_ACTIVITY_TIMEOUT_MS = 2500;
const INITIAL_ACTIVITY_POLL_MS = 50;

type SerialMonitorErrorCode =
  | 'PORT_NOT_SELECTED'
  | 'PERMISSION_DENIED'
  | 'PORT_BUSY'
  | 'PORT_ERROR'
  | 'STREAM_INACTIVE';

class SerialMonitorError extends Error {
  readonly code: SerialMonitorErrorCode;

  constructor(message: string, code: SerialMonitorErrorCode) {
    super(message);
    this.name = 'SerialMonitorError';
    this.code = code;
  }
}

export class WebSerialMonitor implements SerialMonitorAdapter {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private keepReading = true;
  private decoder = new TextDecoder();
  private diagnostics: SerialMonitorDiagnostics = {
    bytesReceived: 0,
    chunksReceived: 0,
    isReading: false,
    portInfo: null,
    lastDataAtMs: null,
  };
  private onDataCallback?: (data: string) => void;
  private onErrorCallback?: (error: Error) => void;

  async connect(baudRate: number = 115200): Promise<void> {
    if (!window.isSecureContext) {
      throw new Error('Secure context required for Web Serial API');
    }
    if (!navigator.serial) {
      throw new Error('Web Serial API not supported in this browser');
    }

    const candidates = await this.selectPortCandidates();
    const shouldRequireInitialData = candidates.length > 1;
    let lastError: Error | null = null;

    for (let i = 0; i < candidates.length; i++) {
      const candidate = candidates[i];
      const isLastCandidate = i === candidates.length - 1;
      try {
        await this.connectToPort(
          candidate,
          baudRate,
          shouldRequireInitialData && !isLastCandidate
        );
        return;
      } catch (error: any) {
        lastError = error;
        await this.disconnect().catch(() => {});
      }
    }

    if (lastError) {
      throw lastError;
    }

    throw new SerialMonitorError('No serial ports available', 'PORT_NOT_SELECTED');
  }

  private async connectToPort(port: SerialPort, baudRate: number, requireInitialData: boolean): Promise<void> {
    await this.openWithRetry(port, baudRate);

    if (!port.readable) {
      await port.close().catch(() => {});
      throw new SerialMonitorError(
        'Connected serial port has no readable stream. Reconnect the device and select the active runtime port.',
        'PORT_ERROR'
      );
    }

    this.port = port;
    this.diagnostics.portInfo = this.safeGetPortInfo(port);
    this.persistPreferredPort(this.diagnostics.portInfo);

    await this.ensureActiveConsoleSignals(port);

    this.keepReading = true;
    this.diagnostics.bytesReceived = 0;
    this.diagnostics.chunksReceived = 0;
    this.diagnostics.lastDataAtMs = null;
    this.decoder = new TextDecoder();
    void this.startReadingLoop();

    if (requireInitialData) {
      await this.waitForInitialActivity(INITIAL_ACTIVITY_TIMEOUT_MS);
    }
  }

  private async selectPortCandidates(): Promise<SerialPort[]> {
    const preferred = this.loadPreferredPortInfo();

    try {
      const grantedPorts = await navigator.serial.getPorts();
      if (grantedPorts.length > 0) {
        const portOrder = [...grantedPorts].sort((a, b) => {
          const aInfo = this.safeGetPortInfo(a);
          const bInfo = this.safeGetPortInfo(b);
          const aPreferred = preferred && aInfo
            ? aInfo.usbVendorId === preferred.usbVendorId && aInfo.usbProductId === preferred.usbProductId
            : false;
          const bPreferred = preferred && bInfo
            ? bInfo.usbVendorId === preferred.usbVendorId && bInfo.usbProductId === preferred.usbProductId
            : false;
          if (aPreferred === bPreferred) return 0;
          return aPreferred ? -1 : 1;
        });

        const validPorts = portOrder.filter((port) => this.safeGetPortInfo(port));
        if (validPorts.length > 0) {
          return validPorts;
        }
      }
    } catch {
      // Fall back to explicit permission request below.
    }

    try {
      return [await navigator.serial.requestPort()];
    } catch (e: any) {
      if (e.name === 'NotFoundError') {
        throw new SerialMonitorError('No port selected by user', 'PORT_NOT_SELECTED');
      }
      if (e.name === 'SecurityError') {
        throw new SerialMonitorError('Permission denied to access serial port', 'PERMISSION_DENIED');
      }
      throw new SerialMonitorError(e?.message ?? 'Failed to access serial port', 'PORT_ERROR');
    }
  }

  private async startReadingLoop(): Promise<void> {
    if (!this.port || !this.port.readable) {
      return;
    }

    this.reader = this.port.readable.getReader();
    this.diagnostics.isReading = true;

    try {
      while (this.keepReading && this.reader) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }

        if (!value) {
          continue;
        }

        this.diagnostics.bytesReceived += value.byteLength;
        this.diagnostics.chunksReceived += 1;
        this.diagnostics.lastDataAtMs = Date.now();

        if (this.onDataCallback) {
          const text = this.decoder.decode(value, { stream: true });
          if (text.length > 0) {
            this.onDataCallback(text);
          }
        }
      }

      if (this.keepReading) {
        throw new SerialMonitorError(
          'Serial stream ended unexpectedly. Device may have reset or switched interfaces.',
          'STREAM_INACTIVE'
        );
      }

      const flush = this.decoder.decode();
      if (flush.length > 0 && this.onDataCallback) {
        this.onDataCallback(flush);
      }
    } catch (error: any) {
      if (this.keepReading && this.onErrorCallback) {
        this.onErrorCallback(error);
      }
    } finally {
      this.diagnostics.isReading = false;
      if (this.reader) {
        this.reader.releaseLock();
        this.reader = null;
      }
    }
  }

  async disconnect(): Promise<void> {
    this.keepReading = false;

    if (this.reader) {
      await this.reader.cancel().catch(() => {});
      this.reader.releaseLock();
      this.reader = null;
    }

    if (this.writer) {
      await this.writer.close().catch(() => {});
      this.writer.releaseLock();
      this.writer = null;
    }

    if (this.port) {
      await this.port.close().catch(() => {});
      this.port = null;
    }

    this.diagnostics.isReading = false;
    this.decoder = new TextDecoder();
  }

  async write(data: string): Promise<void> {
    if (!this.port || !this.port.writable) {
      throw new Error('Port not connected or not writable');
    }

    if (!this.writer) {
      this.writer = this.port.writable.getWriter();
    }

    await this.writer.write(new TextEncoder().encode(data));
  }

  getDiagnostics(): SerialMonitorDiagnostics {
    return { ...this.diagnostics };
  }

  onData(callback: (data: string) => void): void {
    this.onDataCallback = callback;
  }

  onError(callback: (error: Error) => void): void {
    this.onErrorCallback = callback;
  }

  private safeGetPortInfo(port: SerialPort): { usbVendorId?: number; usbProductId?: number } | null {
    try {
      return port.getInfo?.() ?? null;
    } catch {
      return null;
    }
  }

  private loadPreferredPortInfo(): { usbVendorId?: number; usbProductId?: number } | null {
    try {
      const raw = window.localStorage.getItem(PREFERRED_PORT_INFO_KEY);
      if (!raw) return null;
      return JSON.parse(raw);
    } catch {
      return null;
    }
  }

  private persistPreferredPort(portInfo: { usbVendorId?: number; usbProductId?: number } | null) {
    if (!portInfo) return;
    try {
      window.localStorage.setItem(PREFERRED_PORT_INFO_KEY, JSON.stringify(portInfo));
    } catch {
      // Ignore persistence issues; monitor can still operate.
    }
  }

  private async openWithRetry(port: SerialPort, baudRate: number): Promise<void> {
    const attempts = [0, ...PORT_OPEN_RETRY_DELAYS_MS];

    for (let i = 0; i < attempts.length; i++) {
      if (attempts[i] > 0) {
        await this.delay(attempts[i]);
      }

      try {
        await port.open({ baudRate });
        return;
      } catch (error: any) {
        const isLastAttempt = i === attempts.length - 1;
        if (isLastAttempt) {
          if (error?.name === 'SecurityError') {
            throw new SerialMonitorError('Permission denied to access serial port', 'PERMISSION_DENIED');
          }
          throw new SerialMonitorError(
            'Serial port is busy or unavailable. Close other applications using it and retry.',
            'PORT_BUSY'
          );
        }
      }
    }
  }

  private async waitForInitialActivity(timeoutMs: number): Promise<void> {
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
      if (this.diagnostics.bytesReceived > 0) {
        return;
      }
      await this.delay(INITIAL_ACTIVITY_POLL_MS);
    }

    throw new SerialMonitorError(
      `Connected to serial port but no data was received within ${timeoutMs}ms. The selected port may be stale or inactive.`,
      'STREAM_INACTIVE'
    );
  }

  private async ensureActiveConsoleSignals(port: SerialPort): Promise<void> {
    if (typeof port.setSignals !== 'function') {
      return;
    }

    try {
      await port.setSignals({ dataTerminalReady: true, requestToSend: false });
    } catch {
      // Some adapters do not support signal toggling. Reading still works without this.
    }
  }

  private async delay(ms: number): Promise<void> {
    await new Promise((resolve) => setTimeout(resolve, ms));
  }
}
