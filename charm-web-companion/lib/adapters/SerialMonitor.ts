import { SerialMonitorAdapter } from './types';
import { logSerialLifecycleEvent } from '../serialLifecycle';

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
  | 'PORT_DISCONNECTED'
  | 'STALE_PORT'
  | 'STREAM_INACTIVE';

class SerialMonitorError extends Error {
  readonly code: SerialMonitorErrorCode;

  constructor(message: string, code: SerialMonitorErrorCode) {
    super(message);
    this.name = 'SerialMonitorError';
    this.code = code;
  }
}

type PortCandidate = {
  port: SerialPort;
  source: 'granted' | 'requested';
  requireInitialData: boolean;
  index: number;
};

export class WebSerialMonitor implements SerialMonitorAdapter {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private keepReading = true;
  private decoder = new TextDecoder();
  private readLoopPromise: Promise<void> | null = null;
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

    const grantedCandidates = await this.selectGrantedPortCandidates();
    const allCandidates: PortCandidate[] = [...grantedCandidates];
    let lastError: Error | null = null;
    let attemptedRequestedCandidate = false;

    while (true) {
      const candidate = allCandidates.shift();

      if (!candidate) {
        if (attemptedRequestedCandidate) {
          break;
        }

        let requestedCandidate: PortCandidate | null = null;
        try {
          requestedCandidate = await this.requestPortCandidate();
        } catch (error) {
          if (lastError) {
            throw lastError;
          }
          throw error;
        }
        attemptedRequestedCandidate = true;
        if (!requestedCandidate) {
          break;
        }
        allCandidates.push(requestedCandidate);
        continue;
      }

      try {
        await this.connectToPort(candidate, baudRate);
        return;
      } catch (error: any) {
        lastError = error;
        this.logEvent('connect_candidate_failed', {
          code: error?.code ?? 'PORT_ERROR',
          message: error?.message ?? 'Unknown error',
          source: candidate.source,
          candidateIndex: candidate.index,
          portInfo: this.safeGetPortInfo(candidate.port),
        });
        await this.disconnect('candidate_failed').catch(() => {});
      }
    }

    if (lastError) {
      throw lastError;
    }

    throw new SerialMonitorError('No serial ports available', 'PORT_NOT_SELECTED');
  }

  private async connectToPort(candidate: PortCandidate, baudRate: number): Promise<void> {
    const { port, source, requireInitialData, index } = candidate;
    const portInfo = this.safeGetPortInfo(port);
    this.logEvent('open_start', { source, candidateIndex: index, portInfo, baudRate });
    await this.openWithRetry(port, baudRate);
    this.logEvent('open_end', { source, candidateIndex: index, portInfo });

    if (!port.readable) {
      await port.close().catch(() => {});
      throw new SerialMonitorError(
        'Connected serial port has no readable stream. Reconnect the device and select the active runtime port.',
        'STALE_PORT'
      );
    }

    this.port = port;
    this.diagnostics.portInfo = portInfo;
    this.persistPreferredPort(this.diagnostics.portInfo);

    await this.ensureActiveConsoleSignals(port);

    this.keepReading = true;
    this.diagnostics.bytesReceived = 0;
    this.diagnostics.chunksReceived = 0;
    this.diagnostics.lastDataAtMs = null;
    this.decoder = new TextDecoder();
    this.readLoopPromise = this.startReadingLoop();

    if (requireInitialData) {
      await this.waitForInitialActivity(INITIAL_ACTIVITY_TIMEOUT_MS);
    }
  }

  private async selectGrantedPortCandidates(): Promise<PortCandidate[]> {
    const preferred = this.loadPreferredPortInfo();

    try {
      const grantedPorts = await navigator.serial.getPorts();
      const withInfo = grantedPorts
        .map((port) => ({ port, info: this.safeGetPortInfo(port) }))
        .filter((item) => item.info !== null) as Array<{ port: SerialPort; info: { usbVendorId?: number; usbProductId?: number } }>;

      const sorted = withInfo.sort((a, b) => {
        const aPreferred = preferred
          ? a.info.usbVendorId === preferred.usbVendorId && a.info.usbProductId === preferred.usbProductId
          : false;
        const bPreferred = preferred
          ? b.info.usbVendorId === preferred.usbVendorId && b.info.usbProductId === preferred.usbProductId
          : false;
        if (aPreferred === bPreferred) return 0;
        return aPreferred ? -1 : 1;
      });

      return sorted.map((item, index) => ({
        port: item.port,
        source: 'granted',
        requireInitialData: true,
        index,
      }));
    } catch {
      return [];
    }
  }

  private async requestPortCandidate(): Promise<PortCandidate | null> {
    try {
      const requested = await navigator.serial.requestPort();
      return {
        port: requested,
        source: 'requested',
        requireInitialData: false,
        index: 0,
      };
    } catch (e: any) {
      if (e.name === 'NotFoundError') {
        if (this.port || this.diagnostics.bytesReceived > 0) {
          return null;
        }
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
    this.logEvent('reader_start', { portInfo: this.diagnostics.portInfo });

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
      this.logEvent('reader_end', {
        bytesReceived: this.diagnostics.bytesReceived,
        chunksReceived: this.diagnostics.chunksReceived,
        keepReading: this.keepReading,
      });
    }
  }

  async disconnect(reason: string = 'manual'): Promise<void> {
    this.logEvent('disconnect_start', { reason, portInfo: this.diagnostics.portInfo });
    this.keepReading = false;

    if (this.reader) {
      await this.reader.cancel().catch(() => {});
    }

    if (this.readLoopPromise) {
      await this.readLoopPromise.catch(() => {});
      this.readLoopPromise = null;
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
    this.logEvent('disconnect_end', { reason });
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
        if (!isLastAttempt) {
          continue;
        }

        if (error?.name === 'SecurityError') {
          throw new SerialMonitorError('Permission denied to access serial port', 'PERMISSION_DENIED');
        }

        if (error?.name === 'NotFoundError') {
          throw new SerialMonitorError('Serial device is no longer available. Reconnect the device and try again.', 'PORT_DISCONNECTED');
        }

        if (error?.name === 'InvalidStateError') {
          throw new SerialMonitorError('Selected serial port is stale or already open. Select the active runtime port and retry.', 'STALE_PORT');
        }

        if (error?.name === 'NetworkError') {
          throw new SerialMonitorError(
            'Serial port is busy or unavailable. Close other applications using it and retry.',
            'PORT_BUSY'
          );
        }

        throw new SerialMonitorError(error?.message ?? 'Failed to open serial port', 'PORT_ERROR');
      }
    }
  }

  private async waitForInitialActivity(timeoutMs: number): Promise<void> {
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
      if (this.diagnostics.bytesReceived > 0) {
        return;
      }
      if (!this.diagnostics.isReading) {
        throw new SerialMonitorError(
          'Serial stream became unavailable during initial attach. The selected port is likely stale.',
          'STALE_PORT'
        );
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

  private logEvent(event: string, data: Record<string, unknown>) {
    logSerialLifecycleEvent('console', event, data);
  }
}
