import { LocalDraft, MappingDocument, MappingDocumentSchema } from './schema';

export const SUPPORTED_CONFIG_PROTOCOL_VERSION = 2;

export function compileLocalDraftToMappingDocument(draft: LocalDraft): MappingDocument {
  const axisEntries = Object.entries(draft.axes)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([target, axis]) => ({
      target,
      source_index: axis.index,
      scale: axis.scale,
      deadzone: axis.deadzone,
      invert: axis.invert,
    }));

  const buttonEntries = Object.entries(draft.buttons)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([target, button]) => ({
      target,
      source_index: button.index,
    }));

  return MappingDocumentSchema.parse({
    version: 1,
    global: {
      scale: draft.global.scale,
      deadzone: draft.global.deadzone,
      clamp_min: draft.global.clampMin,
      clamp_max: draft.global.clampMax,
    },
    axes: axisEntries,
    buttons: buttonEntries,
  });
}
