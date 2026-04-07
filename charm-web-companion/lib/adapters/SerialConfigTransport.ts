import { ConfigTransportAdapter } from './types';
import { ConfigRequestEnvelope, ConfigResponseEnvelope, ConfigResponseEnvelopeSchema } from '../schema';
import { ConfigError } from '../types';
import {
  getSerialPortIdentity,
  loadPreferredRuntimePort,
  logSerialLifecycleEvent,
  sameSerialPortIdentity,
} from '../serialLifecycle';

const COMMAND_TIMEOUT_MS = 5000;
const POST_OPEN_SETTLE_MS = 400;

/**
 * SerialConfigTransport handles the framing and transport of @CFG: commands
 * over a Web Serial port. It respects the 2048-byte frame limit and
 * handles mixed log streams.
 */
export class SerialConfigTransport implements ConfigTransportAdapter {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<string> | null = null;
  private writer: WritableStreamDefaultWriter<string> | null = null;
  private readableStreamClosed: Promise<void> | null = null;
  private writableStreamClosed: Promise<void> | null = null;
  private keepReading = true;
  private responseWaiters: Map<number, (res: ConfigResponseEnvelope) => void> = new Map();
  private buffer = '';
  private readLoopPromise: Promise<void> | null = null;

  async connect(baudRate: number = 115200): Promise<void> {
    if (!window.isSecureContext) {
      throw new ConfigError('Secure context required for Web Serial', 'INSECURE_CONTEXT');
    }
    if (!navigator.serial) {
      throw new ConfigError('Web Serial API not supported in this browser', 'UNSUPPORTED_BROWSER');
    }

    try {
      const preferred = loadPreferredRuntimePort();
      const ports = await navigator.serial.getPorts();
      if (ports.length > 0) {
        this.port = this.selectBestPort(ports, preferred);
      } else {
        this.port = await navigator.serial.requestPort();
      }
    } catch (e: any) {
      if (e.name === 'NotFoundError') {
        throw new ConfigError('No port selected', 'PORT_NOT_SELECTED');
      }
      if (e.name === 'SecurityError') {
        throw new ConfigError('Permission denied', 'PERMISSION_DENIED');
      }
      throw new ConfigError(`Failed to request port: ${e.message}`, 'PORT_ERROR');
    }

    this.logEvent('open_start', { identity: getSerialPortIdentity(this.port), baudRate });
    try {
      await this.port.open({ baudRate });
      this.logEvent('open_end', { identity: getSerialPortIdentity(this.port), baudRate });
    } catch (e: any) {
      throw new ConfigError('Serial port is busy or unavailable.', 'PORT_BUSY');
    }

    await this.ensureStableSignals(this.port);

    // Set up writer
    const textEncoder = new TextEncoderStream();
    this.writableStreamClosed = textEncoder.readable.pipeTo(this.port.writable!);
    this.writer = textEncoder.writable.getWriter();

    // Set up reader
    this.keepReading = true;
    this.readLoopPromise = this.startReading();
    await this.delay(POST_OPEN_SETTLE_MS);
  }

  async disconnect(): Promise<void> {
    this.logEvent('disconnect_start', { identity: getSerialPortIdentity(this.port) });
    this.keepReading = false;

    if (this.reader) {
      await this.reader.cancel().catch(() => {});
    }

    if (this.readLoopPromise) {
      await this.readLoopPromise.catch(() => {});
      this.readLoopPromise = null;
    }

    if (this.readableStreamClosed) {
      await this.readableStreamClosed.catch(() => {});
      this.readableStreamClosed = null;
    }

    if (this.writer) {
      await Promise.resolve(this.writer.close()).catch(() => {});
      this.writer.releaseLock();
      this.writer = null;
    }

    if (this.writableStreamClosed) {
      await this.writableStreamClosed.catch(() => {});
      this.writableStreamClosed = null;
    }

    if (this.port) {
      await this.port.close().catch(() => {});
      this.port = null;
    }

    this.responseWaiters.clear();
    this.buffer = '';
    this.logEvent('disconnect_end', {});
  }

  private async startReading() {
    if (!this.port || !this.port.readable) return;

    const textDecoder = new TextDecoderStream();
    this.readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.getReader();
    this.logEvent('reader_start', { identity: getSerialPortIdentity(this.port) });

    try {
      while (this.keepReading && this.reader) {
        const result = await this.reader.read();
        if (!result || result.done) {
          this.logEvent('reader_done', { identity: getSerialPortIdentity(this.port) });
          break;
        }

        const value = result.value;
        if (value) {
          this.buffer += value;
          // Process complete lines
          const lines = this.buffer.split('\n');
          this.buffer = lines.pop() || '';
          for (const line of lines) {
            this.parseLine(line);
          }
        }
      }
    } catch (error: any) {
      this.logEvent('reader_error', { message: error?.message ?? 'unknown' });
    } finally {
      if (this.reader) {
        this.reader.releaseLock();
        this.reader = null;
      }
      this.logEvent('reader_end', { identity: getSerialPortIdentity(this.port) });
    }
  }

  async sendCommand(request: ConfigRequestEnvelope): Promise<ConfigResponseEnvelope> {
    if (!this.writer) {
      throw new ConfigError('Not connected to device', 'NOT_CONNECTED');
    }

    const json = JSON.stringify(request);
    const frame = `@CFG:${json}\n`;

    if (frame.length > 2048) {
      throw new ConfigError('Oversized frame rejected (max 2048 bytes)', 'kCapacityExceeded');
    }

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.responseWaiters.delete(request.request_id);
        reject(new ConfigError(`Command ${request.command} timed out (ID: ${request.request_id})`, 'kTimeout'));
      }, COMMAND_TIMEOUT_MS);

      this.responseWaiters.set(request.request_id, (res) => {
        clearTimeout(timeout);
        this.responseWaiters.delete(request.request_id);
        resolve(res);
      });

      this.writer!.write(frame).catch((error: any) => {
        clearTimeout(timeout);
        this.responseWaiters.delete(request.request_id);
        reject(
          new ConfigError(
            `Failed to send ${request.command}: ${error?.message ?? 'transport write failed'}`,
            'kTransportFailure',
          ),
        );
      });
    });
  }

  /**
   * Internal line parser used by the reader loop.
   * It finds and extracts @CFG: frames from the stream.
   */
  private parseLine(line: string): void {
    const trimmed = line.trim();
    if (trimmed.startsWith('@CFG:')) {
      try {
        const jsonStr = trimmed.substring(5).trim();
        const json = JSON.parse(jsonStr);
        const response = ConfigResponseEnvelopeSchema.parse(json);

        const waiter = this.responseWaiters.get(response.request_id);
        if (waiter) {
          waiter(response);
        }
      } catch (e: any) {
        console.error('Failed to parse @CFG frame:', e.message);
      }
    }
  }

  private async ensureStableSignals(port: SerialPort): Promise<void> {
    if (typeof port.setSignals !== 'function') {
      return;
    }

    try {
      // Match the console attach behavior so config commands do not hold
      // ESP32 reset/boot strap lines in an asserted state on USB-UART bridges.
      await port.setSignals({ dataTerminalReady: false, requestToSend: false });
      this.logEvent('signals_deasserted', { identity: getSerialPortIdentity(port) });
    } catch (error: any) {
      this.logEvent('signals_deassert_failed', {
        identity: getSerialPortIdentity(port),
        message: error?.message ?? 'unknown',
      });
    }
  }

  private selectBestPort(ports: SerialPort[], preferred: ReturnType<typeof loadPreferredRuntimePort>): SerialPort {
    if (!preferred) {
      return ports[0];
    }

    const exactMatch = ports.find((port) => sameSerialPortIdentity(getSerialPortIdentity(port), preferred));
    if (exactMatch) {
      return exactMatch;
    }

    const byVidPid = ports.find((port) => {
      const identity = getSerialPortIdentity(port);
      return (
        identity?.usbVendorId === preferred.usbVendorId &&
        identity?.usbProductId === preferred.usbProductId
      );
    });

    return byVidPid ?? ports[0];
  }

  private logEvent(event: string, data: Record<string, unknown>) {
    logSerialLifecycleEvent('config', event, data);
  }

  private async delay(ms: number): Promise<void> {
    await new Promise((resolve) => setTimeout(resolve, ms));
  }

  /**
   * External entry point for parsing chunks (for testing or external log monitors).
   */
  parseStream(chunk: string, onResponse: (response: ConfigResponseEnvelope) => void): void {
    const lines = chunk.split('\n');
    for (const line of lines) {
      const trimmed = line.trim();
      if (trimmed.startsWith('@CFG:')) {
        try {
          const jsonStr = trimmed.substring(5).trim();
          const json = JSON.parse(jsonStr);
          const response = ConfigResponseEnvelopeSchema.parse(json);
          onResponse(response);
        } catch (e: any) {
          console.error('Failed to parse @CFG frame from stream:', e.message);
        }
      }
    }
  }
}
