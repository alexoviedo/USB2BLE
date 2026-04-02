import { SerialMonitorAdapter } from './types';

export class WebSerialMonitor implements SerialMonitorAdapter {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<string> | null = null;
  private writer: WritableStreamDefaultWriter<string> | null = null;
  private readableStreamClosed: Promise<void> | null = null;
  private writableStreamClosed: Promise<void> | null = null;
  private keepReading = true;
  private onDataCallback?: (data: string) => void;
  private onErrorCallback?: (error: Error) => void;

  async connect(baudRate: number = 115200): Promise<void> {
    if (!window.isSecureContext) {
      throw new Error('Secure context required for Web Serial API');
    }
    if (!navigator.serial) {
      throw new Error('Web Serial API not supported in this browser');
    }

    try {
      this.port = await navigator.serial.requestPort();
    } catch (e: any) {
      if (e.name === 'NotFoundError') {
        throw new Error('No port selected by user');
      }
      if (e.name === 'SecurityError') {
        throw new Error('Permission denied to access serial port');
      }
      throw e;
    }

    try {
      await this.port.open({ baudRate });
    } catch (e: any) {
      throw new Error('Serial port is busy or unavailable. Close other applications using it.');
    }

    this.keepReading = true;
    this.startReading();
  }

  private async startReading() {
    if (!this.port || !this.port.readable) return;

    const textDecoder = new TextDecoderStream();
    this.readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.getReader();

    try {
      while (this.keepReading) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        if (value && this.onDataCallback) {
          this.onDataCallback(value);
        }
      }
    } catch (error: any) {
      if (this.onErrorCallback) {
        this.onErrorCallback(error);
      }
    } finally {
      if (this.reader) {
        this.reader.releaseLock();
        this.reader = null;
      }
    }
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
  }

  async write(data: string): Promise<void> {
    if (!this.port || !this.port.writable) {
      throw new Error('Port not connected or not writable');
    }

    if (!this.writer) {
      const textEncoder = new TextEncoderStream();
      this.writableStreamClosed = textEncoder.readable.pipeTo(this.port.writable);
      this.writer = textEncoder.writable.getWriter();
    }

    await this.writer.write(data);
  }

  onData(callback: (data: string) => void): void {
    this.onDataCallback = callback;
  }

  onError(callback: (error: Error) => void): void {
    this.onErrorCallback = callback;
  }
}
