import { ConfigTransportAdapter } from './types';
import { ConfigRequestEnvelope, ConfigResponseEnvelope, ConfigResponseEnvelopeSchema } from '../schema';
import { ConfigError } from '../types';

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

  async connect(baudRate: number = 115200): Promise<void> {
    if (!window.isSecureContext) {
      throw new ConfigError('Secure context required for Web Serial', 'INSECURE_CONTEXT');
    }
    if (!navigator.serial) {
      throw new ConfigError('Web Serial API not supported in this browser', 'UNSUPPORTED_BROWSER');
    }

    try {
      // Try to reuse an already permitted port first
      const ports = await navigator.serial.getPorts();
      if (ports.length > 0) {
        this.port = ports[0];
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

    try {
      await this.port.open({ baudRate });
    } catch (e: any) {
      throw new ConfigError('Serial port is busy or unavailable.', 'PORT_BUSY');
    }

    // Set up writer
    const textEncoder = new TextEncoderStream();
    this.writableStreamClosed = textEncoder.readable.pipeTo(this.port.writable!);
    this.writer = textEncoder.writable.getWriter();

    // Set up reader
    this.keepReading = true;
    this.startReading();
  }

  async disconnect(): Promise<void> {
    this.keepReading = false;

    if (this.reader) {
      await this.reader.cancel();
    }

    if (this.readableStreamClosed) {
      await this.readableStreamClosed.catch(() => {});
    }

    if (this.writer) {
      await this.writer.close();
      this.writer.releaseLock();
    }

    if (this.writableStreamClosed) {
      await this.writableStreamClosed.catch(() => {});
    }

    if (this.port) {
      await this.port.close();
      this.port = null;
    }

    this.responseWaiters.clear();
    this.buffer = '';
  }

  private async startReading() {
    if (!this.port || !this.port.readable) return;

    const textDecoder = new TextDecoderStream();
    this.readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.getReader();

    try {
      while (this.keepReading && this.reader) {
        const result = await this.reader.read();
        if (!result || result.done) break;

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
      console.error('Read error in ConfigTransport', error);
    } finally {
      if (this.reader) {
        this.reader.releaseLock();
        this.reader = null;
      }
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

    await this.writer.write(frame);

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.responseWaiters.delete(request.request_id);
        reject(new ConfigError(`Command ${request.command} timed out (ID: ${request.request_id})`, 'kTimeout'));
      }, 5000);

      this.responseWaiters.set(request.request_id, (res) => {
        clearTimeout(timeout);
        resolve(res);
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
          this.responseWaiters.delete(response.request_id);
        }
      } catch (e: any) {
        console.error('Failed to parse @CFG frame:', e.message);
      }
    }
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
