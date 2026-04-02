import { SerialMonitorAdapter } from './types';

export interface SerialMonitorDiagnostics {
  bytesReceived: number;
  chunksReceived: number;
  isReading: boolean;
  portInfo: { usbVendorId?: number; usbProductId?: number } | null;
}

const PREFERRED_PORT_INFO_KEY = 'charm_preferred_port_info';

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

    const port = await this.selectPort();

    try {
      await port.open({ baudRate });
    } catch {
      throw new Error('Serial port is busy or unavailable. Close other applications using it.');
    }

    if (!port.readable) {
      await port.close().catch(() => {});
      throw new Error('Connected serial port has no readable stream. Reconnect the device and select the active runtime port.');
    }

    this.port = port;
    this.diagnostics.portInfo = this.safeGetPortInfo(port);
    this.persistPreferredPort(this.diagnostics.portInfo);

    this.keepReading = true;
    this.diagnostics.bytesReceived = 0;
    this.diagnostics.chunksReceived = 0;
    this.startReadingLoop();
  }

  private async selectPort(): Promise<SerialPort> {
    const preferred = this.loadPreferredPortInfo();

    try {
      const grantedPorts = await navigator.serial.getPorts();
      if (grantedPorts.length > 0) {
        const preferredPort = preferred
          ? grantedPorts.find((candidate) => {
              const info = this.safeGetPortInfo(candidate);
              return (
                info?.usbVendorId === preferred.usbVendorId &&
                info?.usbProductId === preferred.usbProductId
              );
            })
          : null;

        return preferredPort ?? grantedPorts[0];
      }
    } catch {
      // Fall back to explicit request below.
    }

    try {
      return await navigator.serial.requestPort();
    } catch (e: any) {
      if (e.name === 'NotFoundError') {
        throw new Error('No port selected by user');
      }
      if (e.name === 'SecurityError') {
        throw new Error('Permission denied to access serial port');
      }
      throw e;
    }
  }

  private async startReadingLoop() {
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

        if (this.onDataCallback) {
          const text = this.decoder.decode(value, { stream: true });
          if (text.length > 0) {
            this.onDataCallback(text);
          }
        }
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
}
