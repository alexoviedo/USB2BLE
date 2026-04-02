import { create } from 'zustand';
import { SerialOwner, SerialPermissionState } from './types';

interface AppState {
  serialOwner: SerialOwner;
  serialPermissionState: SerialPermissionState;
  setSerialOwner: (owner: SerialOwner) => void;
  setSerialPermissionState: (state: SerialPermissionState) => void;
  
  // Environment Capabilities
  isSecureContext: boolean;
  hasWebSerial: boolean;
  hasGamepadApi: boolean;
  setCapabilities: (caps: { isSecureContext: boolean; hasWebSerial: boolean; hasGamepadApi: boolean }) => void;
}

export const useAppStore = create<AppState>((set) => ({
  serialOwner: 'none',
  serialPermissionState: 'unknown',
  setSerialOwner: (owner) => set({ serialOwner: owner }),
  setSerialPermissionState: (state) => set({ serialPermissionState: state }),
  
  isSecureContext: false,
  hasWebSerial: false,
  hasGamepadApi: false,
  setCapabilities: (caps) => set(caps),
}));
