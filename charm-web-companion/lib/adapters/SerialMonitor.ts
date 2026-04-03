import { SerialMonitorAdapter } from './types';
import {
  getSerialPortIdentity,
  loadLastFlashPort,
  loadPreferredRuntimePort,
  logSerialLifecycleEvent,
  portIdentityKey,
  sameSerialPortIdentity,
  savePreferredRuntimePort,
  SerialPortIdentity,
} from '../serialLifecycle';

export interface SerialMonitorDiagnostics {
  bytesReceived: number;
  chunksReceived: number;
  isReading: boolean;
  portInfo: SerialPortIdentity | null;
  lastDataAtMs: number | null;
}

const PORT_OPEN_RETRY_DELAYS_MS = [150, 350, 700] as const;
const INITIAL_ACTIVITY_TIMEOUT_MS = 4000;
const INITIAL_ACTIVITY_POLL_MS = 50;
const GRANTED_PORT_REFRESH_DELAYS_MS = [0, 250, 500, 900] as const;
const RECENT_FLASH_WINDOW_MS = 15000;

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
  index: number;
  identity: SerialPortIdentity | null;
  score: number;
  preferredMatch: boolean;
  recentFlash: boolean;
};

export class WebSerialMonitor implements SerialMonitorAdapter {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private keepReading = true;
  private decoder = new TextDecoder();
  private readLoopPromise: Promise<void> | null = null;
  private failedCandidateIdentities = new Set<string>();
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

    this.failedCandidateIdentities.clear();

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
        this.logEvent('candidate_selected', {
          source: candidate.source,
          candidateIndex: candidate.index,
          score: candidate.score,
          preferredMatch: candidate.preferredMatch,
          recentFlash: candidate.recentFlash,
          identity: candidate.identity,
        });

        const connectionOutcome = await this.connectToPort(candidate, baudRate);
        this.logEvent('candidate_connected', {
          source: candidate.source,
          candidateIndex: candidate.index,
          initialStreamState: connectionOutcome.initialStreamState,
          identity: candidate.identity,
        });
        return;
      } catch (error: any) {
        const failedKey = portIdentityKey(candidate.identity);
        this.failedCandidateIdentities.add(failedKey);
        lastError = error;
        this.logEvent('connect_candidate_failed', {
          code: error?.code ?? 'PORT_ERROR',
          message: error?.message ?? 'Unknown error',
          source: candidate.source,
          candidateIndex: candidate.index,
          score: candidate.score,
          identity: candidate.identity,
        });
        await this.disconnect('candidate_failed').catch(() => {});
      }
    }

    if (lastError) {
      throw lastError;
    }

    throw new SerialMonitorError('No serial ports available', 'PORT_NOT_SELECTED');
  }

  private async connectToPort(
    candidate: PortCandidate,
    baudRate: number
  ): Promise<{ initialStreamState: 'active' | 'quiet' }> {
    const { port, source, index } = candidate;
    const portInfo = candidate.identity;
    this.logEvent('open_start', { source, candidateIndex: index, identity: portInfo, baudRate });
    await this.openWithRetry(port, baudRate);
    this.logEvent('open_end', { source, candidateIndex: index, identity: portInfo });

    if (!port.readable) {
      await port.close().catch(() => {});
      throw new SerialMonitorError(
        'Connected serial port has no readable stream. Reconnect the device and select the active runtime port.',
        'STALE_PORT'
      );
    }

    this.port = port;
    this.diagnostics.portInfo = portInfo;

    await this.ensureActiveConsoleSignals(port);

    this.keepReading = true;
    this.diagnostics.bytesReceived = 0;
    this.diagnostics.chunksReceived = 0;
    this.diagnostics.lastDataAtMs = null;
    this.decoder = new TextDecoder();
    this.readLoopPromise = this.startReadingLoop();
    const initialStreamState = await this.waitForInitialActivity(INITIAL_ACTIVITY_TIMEOUT_MS);
    const acceptQuietGrantedCandidate = source === 'granted' && candidate.preferredMatch && !candidate.recentFlash;

    if (
      initialStreamState === 'active' ||
      (initialStreamState === 'quiet' && (source === 'requested' || acceptQuietGrantedCandidate))
    ) {
      savePreferredRuntimePort(this.diagnostics.portInfo);
    }

    if (initialStreamState === 'quiet' && source === 'granted' && !acceptQuietGrantedCandidate) {
      throw new SerialMonitorError(
        'Automatically selected serial interface stayed inactive. Choose the active runtime interface.',
        'STREAM_INACTIVE'
      );
    }

    if (initialStreamState === 'quiet' && acceptQuietGrantedCandidate) {
      this.logEvent('quiet_preferred_granted_candidate_accepted', {
        source,
        candidateIndex: index,
        identity: portInfo,
      });
    }

    return { initialStreamState };
  }

  private async selectGrantedPortCandidates(): Promise<PortCandidate[]> {
    const preferred = loadPreferredRuntimePort();
    const flashContext = loadLastFlashPort();
    const now = Date.now();
    const recentFlash =
      flashContext && Number.isFinite(flashContext.flashedAt)
        ? now - flashContext.flashedAt <= RECENT_FLASH_WINDOW_MS
        : false;

    try {
      const grantedPorts = await this.collectGrantedPorts(recentFlash);

      const scored = grantedPorts.map((port, index) => {
        const identity = getSerialPortIdentity(port);
        const preferredMatch = preferred ? sameSerialPortIdentity(identity, preferred) : false;
        let score = 0;

        if (preferredMatch) {
          score += 120;
        } else if (
          preferred &&
          identity?.usbVendorId === preferred.usbVendorId &&
          identity?.usbProductId === preferred.usbProductId
        ) {
          score += 80;
        }

        if (
          recentFlash &&
          flashContext?.identity &&
          sameSerialPortIdentity(identity, flashContext.identity)
        ) {
          score -= 20;
        }

        if (identity?.serialNumber) {
          score += 15;
        }

        if (identity?.usbVendorId != null && identity?.usbProductId != null) {
          score += 10;
        }

        if (this.failedCandidateIdentities.has(portIdentityKey(identity))) {
          score -= 100;
        }

        return {
          port,
          source: 'granted' as const,
          index,
          identity,
          score,
          preferredMatch,
          recentFlash,
        };
      });

      scored.sort((a, b) => b.score - a.score);

      this.logEvent('granted_candidates_scored', {
        total: scored.length,
        recentFlash,
        candidates: scored.map((candidate) => ({
          index: candidate.index,
          score: candidate.score,
          preferredMatch: candidate.preferredMatch,
          recentFlash: candidate.recentFlash,
          identity: candidate.identity,
        })),
      });

      return scored;
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
        index: 0,
        identity: getSerialPortIdentity(requested),
        score: 0,
        preferredMatch: false,
        recentFlash: false,
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

  private async collectGrantedPorts(recentFlash: boolean): Promise<SerialPort[]> {
    const uniquePorts = new Set<SerialPort>();

    for (const delayMs of GRANTED_PORT_REFRESH_DELAYS_MS) {
      if (delayMs > 0) {
        if (!recentFlash && uniquePorts.size > 0) {
          break;
        }
        await this.delay(delayMs);
      }

      const ports = await navigator.serial.getPorts();
      ports.forEach((port) => uniquePorts.add(port));

      this.logEvent('granted_ports_snapshot', {
        delayMs,
        count: ports.length,
        ports: ports.map((port) => getSerialPortIdentity(port)),
      });

      if (!recentFlash) {
        break;
      }
    }

    return Array.from(uniquePorts);
  }

  private async startReadingLoop(): Promise<void> {
    if (!this.port || !this.port.readable) {
      return;
    }

    this.reader = this.port.readable.getReader();
    this.diagnostics.isReading = true;
    this.logEvent('reader_start', { identity: this.diagnostics.portInfo });

    try {
      while (this.keepReading && this.reader) {
        const { value, done } = await this.reader.read();
        if (done) {
          this.logEvent('reader_done', { reason: 'done_true', identity: this.diagnostics.portInfo });
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
      this.logEvent('reader_error', {
        message: error?.message ?? 'unknown',
        code: error?.code ?? 'PORT_ERROR',
        keepReading: this.keepReading,
        identity: this.diagnostics.portInfo,
      });
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
        identity: this.diagnostics.portInfo,
      });
    }
  }

  async disconnect(reason: string = 'manual'): Promise<void> {
    this.logEvent('disconnect_start', { reason, identity: this.diagnostics.portInfo });
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

  private async waitForInitialActivity(timeoutMs: number): Promise<'active' | 'quiet'> {
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
      if (this.diagnostics.bytesReceived > 0) {
        return 'active';
      }
      if (!this.diagnostics.isReading) {
        throw new SerialMonitorError(
          'Serial stream became unavailable during initial attach. The selected port is likely stale.',
          'STALE_PORT'
        );
      }
      await this.delay(INITIAL_ACTIVITY_POLL_MS);
    }

    this.logEvent('initial_activity_timeout', {
      timeoutMs,
      bytesReceived: this.diagnostics.bytesReceived,
      chunksReceived: this.diagnostics.chunksReceived,
      identity: this.diagnostics.portInfo,
      reason: 'no_bytes_before_timeout',
    });
    return 'quiet';
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
