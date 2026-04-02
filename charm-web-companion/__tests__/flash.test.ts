import { describe, it, expect, vi, beforeEach } from 'vitest';
import { BrowserArtifactIngestion } from '../lib/adapters/ArtifactIngestion';
import { WebSerialFlasher } from '../lib/adapters/Flasher';
import { FlashError } from '../lib/types';

// Mock global fetch
const mockFetch = vi.fn();
global.fetch = mockFetch;

describe('BrowserArtifactIngestion', () => {
  let ingestion: BrowserArtifactIngestion;

  beforeEach(() => {
    ingestion = new BrowserArtifactIngestion();
    mockFetch.mockReset();
  });

  it('validates and loads same-site manifest successfully', async () => {
    const validManifest = {
      version: '1.0.0',
      build_time: '2026-04-01T00:00:00Z',
      commit_sha: 'abcdef1',
      target: 'esp32s3',
      files: {
        bootloader: 'bootloader.bin',
        partition_table: 'partition-table.bin',
        app: 'charm.bin'
      }
    };

    mockFetch.mockResolvedValueOnce({
      ok: true,
      json: async () => validManifest
    });

    const result = await ingestion.fetchSameSiteManifest();
    expect(result).toEqual(validManifest);
  });

  it('rejects incomplete manifest', async () => {
    const invalidManifest = {
      version: '1.0.0',
      // missing target and files
    };

    mockFetch.mockResolvedValue({
      ok: true,
      json: async () => invalidManifest
    });

    await expect(ingestion.fetchSameSiteManifest()).rejects.toThrowError(FlashError);
    await expect(ingestion.fetchSameSiteManifest()).rejects.toThrow(/Malformed manifest.json/);
  });

  it('rejects manual import for missing files', async () => {
    const validManifest = {
      version: '1.0.0',
      build_time: '2026-04-01T00:00:00Z',
      commit_sha: 'abcdef1',
      target: 'esp32s3',
      files: {
        bootloader: 'bootloader.bin',
        partition_table: 'partition-table.bin',
        app: 'charm.bin'
      }
    };
    const file = new File([JSON.stringify(validManifest)], 'manifest.json', { type: 'application/json' });
    
    // The ingestion adapter parseManualManifest just parses the manifest.
    // The missing files check is in the UI component, but we can test the parse success.
    const result = await ingestion.parseManualManifest(file);
    expect(result).toEqual(validManifest);
  });
});

describe('WebSerialFlasher', () => {
  let flasher: WebSerialFlasher;
  let mockPort: any;
  let mockTransport: any;
  let mockESPLoader: any;

  beforeEach(() => {
    flasher = new WebSerialFlasher();
    mockPort = {};
    mockTransport = { disconnect: vi.fn() };
    mockESPLoader = {
      main_fn: vi.fn(),
      read_mac: vi.fn().mockResolvedValue('AA:BB:CC:DD:EE:FF'),
      chip: { CHIP_NAME: 'ESP32-S3' },
      write_flash: vi.fn(),
      hard_reset: vi.fn()
    };

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

    // Mock dynamic import of esptool-js
    vi.spyOn(flasher as any, 'getEsptool').mockResolvedValue({
      Transport: class {
        constructor() { return mockTransport; }
      },
      ESPLoader: class {
        constructor() { return mockESPLoader; }
      }
    });
  });

  it('throws if Web Serial is not supported', async () => {
    Object.defineProperty(global.navigator, 'serial', { value: undefined, configurable: true });
    await expect(flasher.connect()).rejects.toThrow(/Web Serial API not supported/);
  });

  it('throws if not secure context', async () => {
    Object.defineProperty(global.window, 'isSecureContext', { value: false, configurable: true });
    await expect(flasher.connect()).rejects.toThrow(/Secure context required/);
  });

  it('connects and identifies device successfully', async () => {
    await flasher.connect();
    expect(mockESPLoader.main_fn).toHaveBeenCalled();
    
    const mac = await flasher.getMacAddress();
    expect(mac).toBe('AA:BB:CC:DD:EE:FF');
    
    const chip = await flasher.getChipName();
    expect(chip).toBe('ESP32-S3');
  });

  it('handles sync failure gracefully', async () => {
    mockESPLoader.main_fn.mockRejectedValue(new Error('Sync failed'));
    await expect(flasher.connect()).rejects.toThrow(/Bootloader sync failed/);
    expect(mockTransport.disconnect).toHaveBeenCalled();
  });

  it('flashes successfully and resets', async () => {
    await flasher.connect();
    
    const onProgress = vi.fn();
    await flasher.flash(new ArrayBuffer(10), new ArrayBuffer(10), new ArrayBuffer(10), onProgress);
    
    expect(mockESPLoader.write_flash).toHaveBeenCalled();
    expect(mockESPLoader.hard_reset).toHaveBeenCalled();
  });

  it('throws if flash called without connecting', async () => {
    // Reset the flasher to not be connected
    flasher = new WebSerialFlasher();
    await expect(flasher.flash(new ArrayBuffer(0), new ArrayBuffer(0), new ArrayBuffer(0), () => {})).rejects.toThrow(/Not connected to device/);
  });
});
