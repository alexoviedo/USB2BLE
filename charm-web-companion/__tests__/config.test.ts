import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { SerialConfigTransport } from '../lib/adapters/SerialConfigTransport';
import { ConfigError } from '../lib/types';
import { ConfigPersistPayloadSchema, LocalDraftSchema, MappingDocumentSchema } from '../lib/schema';
import { compileLocalDraftToMappingDocument, SUPPORTED_CONFIG_PROTOCOL_VERSION } from '../lib/configCompiler';

describe('SerialConfigTransport', () => {
  let transport: SerialConfigTransport;
  let mockPort: any;
  let mockReader: any;
  let mockWriter: any;

  beforeEach(() => {
    transport = new SerialConfigTransport();

    mockReader = {
      read: vi.fn(),
      releaseLock: vi.fn(),
      cancel: vi.fn()
    };

    mockWriter = {
      write: vi.fn().mockResolvedValue(undefined),
      releaseLock: vi.fn(),
      close: vi.fn()
    };

    mockPort = {
      open: vi.fn().mockResolvedValue(undefined),
      close: vi.fn().mockResolvedValue(undefined),
      setSignals: vi.fn().mockResolvedValue(undefined),
      readable: { pipeTo: vi.fn().mockResolvedValue(undefined) },
      writable: { getWriter: () => mockWriter }
    };

    // Mock global objects
    global.TextDecoderStream = class {
      readable = { getReader: () => mockReader };
      writable = {};
    } as any;

    global.TextEncoderStream = class {
      readable = { pipeTo: vi.fn() };
      writable = { getWriter: () => mockWriter };
    } as any;

    Object.defineProperty(global.navigator, 'serial', {
      value: {
        requestPort: vi.fn().mockResolvedValue(mockPort),
        getPorts: vi.fn().mockResolvedValue([])
      },
      configurable: true
    });

    Object.defineProperty(global.window, 'isSecureContext', {
      value: true,
      configurable: true
    });
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it('connects and opens port at 115200', async () => {
    mockReader.read.mockResolvedValue({ value: undefined, done: true });
    await transport.connect(115200);
    expect(mockPort.open).toHaveBeenCalledWith({ baudRate: 115200 });
    expect(mockPort.setSignals).toHaveBeenCalledWith({ dataTerminalReady: false, requestToSend: false });
  });


  it('disconnect closes port even when reader cancel throws', async () => {
    mockReader.read.mockResolvedValue({ value: undefined, done: true });
    mockReader.cancel.mockRejectedValue(new Error('cancel failed'));
    await transport.connect();
    await transport.disconnect();

    expect(mockPort.close).toHaveBeenCalled();
    expect(mockWriter.releaseLock).toHaveBeenCalled();
  });

  it('rejects oversized frames', async () => {
    await transport.connect();
    const largePayload = 'a'.repeat(2100);
    const request: any = { protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION, request_id: 1, command: 'config.load', payload: { largePayload }, integrity: 'CFG1' };

    await expect(transport.sendCommand(request)).rejects.toThrow(/Oversized frame/);
  });

  it('resolves immediate get_capabilities responses without dropping the waiter', async () => {
    mockReader.read.mockResolvedValue({ value: undefined, done: true });
    await transport.connect();

    const request: any = { protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION, request_id: 7, command: 'config.get_capabilities', payload: {}, integrity: 'CFG1' };

    const responseJson = JSON.stringify({
      protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION,
      request_id: 7,
      command: 'config.get_capabilities',
      status: 'kOk',
      capabilities: { protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION, supports_persist: true, supports_load: true, supports_clear: true, supports_get_capabilities: true, supports_ble_transport: false }
    });

    mockWriter.write.mockImplementationOnce(async () => {
      (transport as any).parseLine(`@CFG:${responseJson}`);
    });

    const result: any = await transport.sendCommand(request);
    expect(result.status).toBe('kOk');
    expect(result.request_id).toBe(7);
    expect(result.capabilities?.supports_get_capabilities).toBe(true);
  });

  it('times out cleanly when no response arrives', async () => {
    vi.useFakeTimers();
    mockReader.read.mockResolvedValue({ value: undefined, done: true });

    const connectPromise = transport.connect();
    await vi.advanceTimersByTimeAsync(400);
    await connectPromise;

    const request: any = {
      protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION,
      request_id: 11,
      command: 'config.get_capabilities',
      payload: {},
      integrity: 'CFG1'
    };

    const responsePromise = transport.sendCommand(request);
    const rejection = expect(responsePromise).rejects.toThrow(/timed out/);
    expect((transport as any).responseWaiters.size).toBe(1);

    await vi.advanceTimersByTimeAsync(5000);
    await rejection;
    expect((transport as any).responseWaiters.size).toBe(0);
  });

  it('ignores non-@CFG log lines', async () => {
    const onResponse = vi.fn();
    transport.parseStream(`normal log line\n@CFG:{"protocol_version":${SUPPORTED_CONFIG_PROTOCOL_VERSION},"request_id":1,"command":"config.load","status":"kOk"}\n`, onResponse);
    expect(onResponse).toHaveBeenCalled();
  });

  it('handles malformed @CFG frames gracefully', async () => {
    const onResponse = vi.fn();
    const consoleSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
    transport.parseStream('@CFG:{"invalid": json}\n', onResponse);
    expect(onResponse).not.toHaveBeenCalled();
    expect(consoleSpy).toHaveBeenCalled();
    consoleSpy.mockRestore();
  });

  it('sends persist requests and resolves their responses', async () => {
    mockReader.read.mockResolvedValue({ value: undefined, done: true });
    await transport.connect();

    const mappingDocument = {
      version: 1,
      global: { scale: 1, deadzone: 0.08, clamp_min: -1, clamp_max: 1 },
      axes: [{ target: 'move_x', source_index: 0, scale: 1, deadzone: 0.08, invert: false }],
      buttons: [{ target: 'action_a', source_index: 0 }]
    };
    const request: any = {
      protocol_version: SUPPORTED_CONFIG_PROTOCOL_VERSION,
      request_id: 8,
      command: 'config.persist',
      payload: {
        mapping_document: mappingDocument,
        profile_id: 1,
        bonding_material: [1, 2, 3]
      },
      integrity: 'CFG1'
    };

    const expectedFrame =
      '@CFG:{"protocol_version":2,"request_id":8,"command":"config.persist","payload":{"mapping_document":{"version":1,"global":{"scale":1,"deadzone":0.08,"clamp_min":-1,"clamp_max":1},"axes":[{"target":"move_x","source_index":0,"scale":1,"deadzone":0.08,"invert":false}],"buttons":[{"target":"action_a","source_index":0}]},"profile_id":1,"bonding_material":[1,2,3]},"integrity":"CFG1"}\n';
    mockWriter.write.mockImplementationOnce(async (frame: string) => {
      expect(frame).toBe(expectedFrame);
      (transport as any).parseLine(
        '@CFG:{"protocol_version":2,"request_id":8,"command":"config.persist","status":"kOk","payload":{"mapping_bundle":{"bundle_id":4,"version":2,"integrity":77},"profile_id":1}}\n'
      );
    });

    const result: any = await transport.sendCommand(request);
    expect(result.status).toBe('kOk');
    expect(result.payload).toEqual({
      mapping_bundle: { bundle_id: 4, version: 2, integrity: 77 },
      profile_id: 1,
    });
  });

  it('accepts persist payloads for supported profile 2', () => {
    expect(
      ConfigPersistPayloadSchema.parse({
        mapping_document: {
          version: 1,
          global: { scale: 1, deadzone: 0.08, clamp_min: -1, clamp_max: 1 },
          axes: [],
          buttons: [],
        },
        profile_id: 2,
      }).profile_id,
    ).toBe(2);
  });

  it('rejects unsupported persist profile ids', () => {
    expect(() =>
      ConfigPersistPayloadSchema.parse({
        mapping_document: {
          version: 1,
          global: { scale: 1, deadzone: 0.08, clamp_min: -1, clamp_max: 1 },
          axes: [],
          buttons: [],
        },
        profile_id: 99,
      }),
    ).toThrow(/supported profiles/);
  });
});

describe('LocalDraftSchema', () => {
  it('validates a correct draft', () => {
    const draft = {
      metadata: { name: 'test', author: 'me', revision: 1, notes: '', updatedAt: '' },
      global: { scale: 1.0, deadzone: 0.1, clampMin: -1.0, clampMax: 1.0 },
      axes: { x: { index: 0, scale: 1, deadzone: 0.08, invert: false } },
      buttons: { a: { index: 0 } }
    };
    expect(LocalDraftSchema.safeParse(draft).success).toBe(true);
  });

  it('rejects invalid clamp ranges', () => {
    const draft = {
      metadata: { name: 'test', author: 'me', revision: 1, notes: '', updatedAt: '' },
      global: { scale: 1.0, deadzone: 0.1, clampMin: 1.0, clampMax: -1.0 },
      axes: {},
      buttons: {}
    };
    const result = LocalDraftSchema.safeParse(draft);
    expect(result.success).toBe(false);
  });

  it('rejects out of range values', () => {
    const draft = {
      metadata: { name: 'test', author: 'me', revision: 1, notes: '', updatedAt: '' },
      global: { scale: 5.0, deadzone: 0.1, clampMin: -1.0, clampMax: 1.0 },
      axes: {},
      buttons: {}
    };
    expect(LocalDraftSchema.safeParse(draft).success).toBe(false);
  });
});

describe('compileLocalDraftToMappingDocument', () => {
  it('builds a deterministic mapping document from a local draft', () => {
    const draft = {
      metadata: { name: 'test', author: 'me', revision: 1, notes: '', updatedAt: '' },
      global: { scale: 1.0, deadzone: 0.1, clampMin: -1.0, clampMax: 1.0 },
      axes: {
        look_y: { index: 2, scale: 0.5, deadzone: 0.02, invert: true },
        move_x: { index: 0, scale: 1, deadzone: 0.08, invert: false }
      },
      buttons: {
        menu: { index: 9 },
        action_a: { index: 0 }
      }
    };

    const result = compileLocalDraftToMappingDocument(draft);

    expect(MappingDocumentSchema.parse(result)).toEqual({
      version: 1,
      global: { scale: 1, deadzone: 0.1, clamp_min: -1, clamp_max: 1 },
      axes: [
        { target: 'look_y', source_index: 2, scale: 0.5, deadzone: 0.02, invert: true },
        { target: 'move_x', source_index: 0, scale: 1, deadzone: 0.08, invert: false }
      ],
      buttons: [
        { target: 'action_a', source_index: 0 },
        { target: 'menu', source_index: 9 }
      ]
    });
  });

  it('rejects compiler-unsupported source indices', () => {
    const draft = {
      metadata: { name: 'test', author: 'me', revision: 1, notes: '', updatedAt: '' },
      global: { scale: 1.0, deadzone: 0.1, clampMin: -1.0, clampMax: 1.0 },
      axes: { move_x: { index: 99, scale: 1, deadzone: 0.08, invert: false } },
      buttons: {}
    };

    expect(() => compileLocalDraftToMappingDocument(draft)).toThrow();
  });
});
