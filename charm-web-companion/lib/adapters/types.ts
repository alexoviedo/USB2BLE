import { Manifest, ConfigRequestEnvelope, ConfigResponseEnvelope } from '../schema';

export interface CapabilitiesAdapter {
  isSecureContext: boolean;
  hasWebSerial: boolean;
  hasGamepadApi: boolean;
}

export interface ArtifactIngestionAdapter {
  fetchSameSiteManifest(): Promise<Manifest>;
  fetchSameSiteBinary(filename: string): Promise<ArrayBuffer>;
  parseManualManifest(file: File): Promise<Manifest>;
  readManualBinary(file: File): Promise<ArrayBuffer>;
}

export interface FlasherAdapter {
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  flash(
    bootloader: ArrayBuffer,
    partitionTable: ArrayBuffer,
    app: ArrayBuffer,
    onProgress: (progress: number) => void
  ): Promise<void>;
  getMacAddress(): Promise<string | null>;
  getChipName(): Promise<string | null>;
}

export interface SerialMonitorAdapter {
  connect(baudRate?: number): Promise<void>;
  disconnect(): Promise<void>;
  write(data: string): Promise<void>;
  onData(callback: (data: string) => void): void;
  onError(callback: (error: Error) => void): void;
}

export interface ConfigTransportAdapter {
  sendCommand(request: ConfigRequestEnvelope): Promise<ConfigResponseEnvelope>;
  parseStream(chunk: string, onResponse: (response: ConfigResponseEnvelope) => void): void;
}

export interface GamepadValidationAdapter {
  getGamepads(): (Gamepad | null)[];
  onGamepadConnected(callback: (e: GamepadEvent) => void): void;
  onGamepadDisconnected(callback: (e: GamepadEvent) => void): void;
}
