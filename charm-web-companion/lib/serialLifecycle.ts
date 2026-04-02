export type SerialLifecycleOwner = 'flash' | 'console' | 'config' | 'store';

export function logSerialLifecycleEvent(
  owner: SerialLifecycleOwner,
  event: string,
  details: Record<string, unknown> = {}
): void {
  try {
    const payload = {
      at: new Date().toISOString(),
      owner,
      event,
      ...details,
    };
    console.info('[serial:lifecycle]', payload);
  } catch {
    // no-op for logging failures
  }
}
