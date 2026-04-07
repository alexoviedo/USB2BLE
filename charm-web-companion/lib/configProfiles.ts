export const SUPPORTED_CONFIG_PROFILES = [
  {
    id: 1,
    name: 'Generic BLE Gamepad',
    summary: 'Backward-compatible default BLE gamepad contract.',
  },
  {
    id: 2,
    name: 'Wireless Xbox Controller',
    summary: 'Xbox-compatible BLE contract for the supported profile lane.',
  },
] as const;

export type SupportedConfigProfile = (typeof SUPPORTED_CONFIG_PROFILES)[number];
export type SupportedConfigProfileId = SupportedConfigProfile['id'];

export const DEFAULT_CONFIG_PROFILE_ID: SupportedConfigProfileId =
  SUPPORTED_CONFIG_PROFILES[0].id;

export function isSupportedConfigProfileId(
  value: number,
): value is SupportedConfigProfileId {
  return SUPPORTED_CONFIG_PROFILES.some((profile) => profile.id === value);
}

export function getSupportedConfigProfile(
  value: number | null | undefined,
): SupportedConfigProfile | null {
  if (typeof value !== 'number') {
    return null;
  }

  return SUPPORTED_CONFIG_PROFILES.find((profile) => profile.id === value) ?? null;
}
