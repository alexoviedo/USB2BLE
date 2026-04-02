import { z } from 'zod';

// ==========================================
// Firmware Artifact Manifest Schema
// ==========================================
export const ManifestSchema = z.object({
  version: z.string(),
  build_time: z.string(),
  commit_sha: z.string(),
  target: z.literal('esp32s3'),
  files: z.object({
    bootloader: z.string(),
    partition_table: z.string(),
    app: z.string(),
  }),
});

export type Manifest = z.infer<typeof ManifestSchema>;

// ==========================================
// Config Transport Schemas
// ==========================================
export const ConfigCommandSchema = z.enum([
  'config.persist',
  'config.load',
  'config.clear',
  'config.get_capabilities',
]);

export const ConfigStatusSchema = z.enum([
  'kUnspecified',
  'kOk',
  'kRejected',
  'kUnavailable',
  'kFailed',
]);

export const ConfigFaultCategorySchema = z.enum([
  'kInvalidRequest',
  'kInvalidState',
  'kUnsupportedCapability',
  'kContractViolation',
  'kResourceExhausted',
  'kCapacityExceeded',
  'kTimeout',
  'kIntegrityFailure',
  'kPersistenceFailure',
  'kAdapterFailure',
  'kTransportFailure',
  'kDeviceProtocolFailure',
  'kConfigurationRejected',
  'kRecoveryRequired',
]);

export const ConfigFaultSchema = z.object({
  category: ConfigFaultCategorySchema,
  reason: z.number(),
});

export const MappingBundleRefSchema = z.object({
  bundle_id: z.number(),
  version: z.number(),
  integrity: z.number(),
});

export const CapabilitiesPayloadSchema = z.object({
  protocol_version: z.number(),
  supports_persist: z.boolean(),
  supports_load: z.boolean(),
  supports_clear: z.boolean(),
  supports_get_capabilities: z.boolean(),
  supports_ble_transport: z.boolean(),
});

export const ConfigRequestEnvelopeSchema = z.object({
  protocol_version: z.number(),
  request_id: z.number().positive(),
  command: ConfigCommandSchema,
  payload: z.any().optional(),
  integrity: z.literal('CFG1'),
});

export const ConfigResponseEnvelopeSchema = z.object({
  protocol_version: z.number(),
  request_id: z.number(),
  command: ConfigCommandSchema,
  status: ConfigStatusSchema,
  fault: ConfigFaultSchema.optional(),
  payload: z.any().optional(),
  capabilities: CapabilitiesPayloadSchema.optional(),
});

export type ConfigRequestEnvelope = z.infer<typeof ConfigRequestEnvelopeSchema>;
export type ConfigResponseEnvelope = z.infer<typeof ConfigResponseEnvelopeSchema>;

// ==========================================
// Local Draft Schema
// ==========================================
export const AxisSchema = z.object({
  index: z.number().int().nonnegative(),
  scale: z.number().min(0.1).max(4.0),
  deadzone: z.number().min(0.0).max(0.95),
  invert: z.boolean(),
});

export const ButtonSchema = z.object({
  index: z.number().int().nonnegative(),
});

export const LocalDraftSchema = z.object({
  metadata: z.object({
    name: z.string().min(1),
    author: z.string(),
    revision: z.number().int().min(1),
    notes: z.string(),
    updatedAt: z.string(),
  }),
  global: z.object({
    scale: z.number().min(0.1).max(4.0),
    deadzone: z.number().min(0.0).max(0.95),
    clampMin: z.number().min(-1.0).max(1.0),
    clampMax: z.number().min(-1.0).max(1.0),
  }).refine(data => data.clampMin < data.clampMax, {
    message: "clampMin must be strictly less than clampMax",
    path: ["clampMin"]
  }),
  axes: z.record(z.string(), AxisSchema),
  buttons: z.record(z.string(), ButtonSchema),
});

export type LocalDraft = z.infer<typeof LocalDraftSchema>;
