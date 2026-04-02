import { GamepadValidationAdapter } from './types';

export class BrowserGamepad implements GamepadValidationAdapter {
  getGamepads(): (Gamepad | null)[] {
    if (typeof navigator !== 'undefined' && navigator.getGamepads) {
      return Array.from(navigator.getGamepads());
    }
    return [];
  }

  onGamepadConnected(callback: (e: GamepadEvent) => void): void {
    if (typeof window !== 'undefined') {
      window.addEventListener('gamepadconnected', callback);
    }
  }

  onGamepadDisconnected(callback: (e: GamepadEvent) => void): void {
    if (typeof window !== 'undefined') {
      window.addEventListener('gamepaddisconnected', callback);
    }
  }

  // Cleanup helper
  removeListeners(connectedCb: (e: GamepadEvent) => void, disconnectedCb: (e: GamepadEvent) => void) {
    if (typeof window !== 'undefined') {
      window.removeEventListener('gamepadconnected', connectedCb);
      window.removeEventListener('gamepaddisconnected', disconnectedCb);
    }
  }
}
