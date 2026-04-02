interface SerialPort {
  open(options: any): Promise<void>;
  close(): Promise<void>;
  readable: ReadableStream;
  writable: WritableStream;
  getInfo(): any;
  setSignals?(signals: { dataTerminalReady?: boolean; requestToSend?: boolean }): Promise<void>;
}

interface Serial {
  requestPort(options?: any): Promise<SerialPort>;
  getPorts(): Promise<SerialPort[]>;
}

interface Navigator {
  serial: Serial;
}

interface Window {
  isSecureContext: boolean;
}
