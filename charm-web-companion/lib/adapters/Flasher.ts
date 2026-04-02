import { FlasherAdapter } from './types';
import { FlashError } from '../types';

const MAC_REGEX = /(?:^|\b)([0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5})(?:\b|$)/;

export class WebSerialFlasher implements FlasherAdapter {
  private transport: any = null;
  private esploader: any = null;
  private detectedMac: string | null = null;

  private async getEsptool() {
    try {
      return await import('esptool-js');
    } catch (err) {
      throw new FlashError('Failed to load esptool-js module', 'ADAPTER_FAILURE');
    }
  }

  async connect(): Promise<void> {
    if (!navigator.serial) {
      throw new FlashError('Web Serial API not supported', 'UNSUPPORTED_BROWSER');
    }

    if (!window.isSecureContext) {
      throw new FlashError('Secure context required for Web Serial', 'INSECURE_CONTEXT');
    }

    const { Transport, ESPLoader } = await this.getEsptool();

    let port: SerialPort;
    try {
      port = await navigator.serial.requestPort();
    } catch (err: any) {
      if (err.name === 'NotFoundError') {
        throw new FlashError('No port selected', 'PORT_NOT_SELECTED');
      }
      if (err.name === 'SecurityError') {
        throw new FlashError('Permission denied', 'PERMISSION_DENIED');
      }
      throw new FlashError(`Failed to request port: ${err.message}`, 'PORT_ERROR');
    }

    try {
      this.transport = new Transport(port);
      this.detectedMac = null;

      const terminal = {
        clean() {},
        writeLine: (data: string) => {
          this.captureMacFromText(data);
          console.log('[esptool]', data);
        },
        write: (data: string) => {
          this.captureMacFromText(data);
          console.log('[esptool]', data);
        }
      };

      const flashOptions = {
        transport: this.transport,
        baudrate: 115200,
        terminal
      };

      this.esploader = new ESPLoader(flashOptions);

      // Handle main_fn vs mainFn defensively
      if (typeof this.esploader.main_fn === 'function') {
        await this.esploader.main_fn();
      } else if (typeof this.esploader.mainFn === 'function') {
        await this.esploader.mainFn();
      } else if (typeof this.esploader.main === 'function') {
        await this.esploader.main();
      } else {
        throw new Error('Could not find ESPLoader main function');
      }

    } catch (err: any) {
      // Clean up if sync fails
      if (this.transport) {
        try { await this.transport.disconnect(); } catch (e) {}
        this.transport = null;
      }
      this.esploader = null;

      if (err.message?.includes('Failed to open serial port') || err.name === 'NetworkError') {
        throw new FlashError('Serial port is busy or unavailable. Close other tabs/apps.', 'PORT_BUSY');
      }

      throw new FlashError(`Bootloader sync failed: ${err.message || 'Unknown error'}. Try holding BOOT and pressing EN/RST.`, 'SYNC_FAILURE');
    }
  }

  async disconnect(): Promise<void> {
    if (this.transport) {
      try {
        await this.transport.disconnect();
      } catch (err) {
        console.error('Error disconnecting transport', err);
      }
      this.transport = null;
    }
    this.esploader = null;
  }

  async flash(
    bootloader: ArrayBuffer,
    partitionTable: ArrayBuffer,
    app: ArrayBuffer,
    onProgress: (progress: number) => void
  ): Promise<void> {
    if (!this.esploader) {
      throw new FlashError('Not connected to device', 'NOT_CONNECTED');
    }

    try {
      const fileArray = [
        { data: new Uint8Array(bootloader), address: 0x0000 },
        { data: new Uint8Array(partitionTable), address: 0x8000 },
        { data: new Uint8Array(app), address: 0x10000 }
      ];

      const flashOptions = {
        fileArray,
        flashSize: 'keep',
        eraseAll: false,
        compress: true,
        reportProgress: (fileIndex: number, written: number, total: number) => {
          const totalBytes = fileArray.reduce((acc, f) => acc + f.data.length, 0);
          let bytesWrittenSoFar = 0;
          for (let i = 0; i < fileIndex; i++) {
            bytesWrittenSoFar += fileArray[i].data.length;
          }
          bytesWrittenSoFar += written;
          const progress = Math.round((bytesWrittenSoFar / totalBytes) * 100);
          onProgress(Math.min(100, progress));
        }
      };

      // Handle write_flash vs writeFlash defensively
      if (typeof this.esploader.write_flash === 'function') {
        await this.esploader.write_flash(flashOptions);
      } else if (typeof this.esploader.writeFlash === 'function') {
        await this.esploader.writeFlash(flashOptions);
      } else {
        throw new Error('Could not find ESPLoader write_flash function');
      }

      // Reset after success
      if (typeof this.esploader.hard_reset === 'function') {
        await this.esploader.hard_reset();
      } else if (typeof this.esploader.hardReset === 'function') {
        await this.esploader.hardReset();
      }

    } catch (err: any) {
      if (err.message?.includes('disconnect') || err.message?.includes('NetworkError')) {
        throw new FlashError('Device unplugged or disconnected mid-flash', 'UNPLUGGED');
      }
      throw new FlashError(`Flash failed: ${err.message}`, 'FLASH_FAILED');
    }
  }

  async getMacAddress(): Promise<string | null> {
    if (!this.esploader) return this.detectedMac;

    if (this.detectedMac) {
      return this.detectedMac;
    }

    try {
      if (this.esploader.chip && typeof this.esploader.chip.readMac === 'function') {
        const mac = await this.esploader.chip.readMac(this.esploader);
        return this.normalizeMac(mac);
      }

      if (typeof this.esploader.read_mac === 'function') {
        const mac = await this.esploader.read_mac();
        return this.normalizeMac(mac);
      }

      if (typeof this.esploader.readMac === 'function') {
        const mac = await this.esploader.readMac();
        return this.normalizeMac(mac);
      }
    } catch (err) {
      console.warn('Failed to read MAC address', err);
    }
    return this.detectedMac;
  }

  async getChipName(): Promise<string | null> {
    if (!this.esploader) return null;
    try {
      if (this.esploader.chip && this.esploader.chip.CHIP_NAME) {
        return this.esploader.chip.CHIP_NAME;
      }
    } catch (err) {
      console.warn('Failed to read chip name', err);
    }
    return null;
  }

  private captureMacFromText(data: string) {
    if (!data) return;
    const match = data.match(MAC_REGEX);
    if (match?.[1]) {
      this.detectedMac = this.normalizeMac(match[1]);
    }
  }

  private normalizeMac(mac: unknown): string | null {
    if (typeof mac !== 'string') {
      return null;
    }

    const cleaned = mac.trim().replace(/-/g, ':').toUpperCase();
    if (/^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(cleaned)) {
      this.detectedMac = cleaned;
      return cleaned;
    }

    const compact = cleaned.replace(/[^0-9A-F]/g, '');
    if (compact.length === 12) {
      const normalized = compact.match(/.{1,2}/g)?.join(':') ?? null;
      if (normalized) {
        this.detectedMac = normalized;
      }
      return normalized;
    }

    return null;
  }
}
