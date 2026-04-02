'use client';

import { useEffect } from 'react';
import { useAppStore } from '@/lib/store';
import { AlertTriangle, CheckCircle2, Info } from 'lucide-react';

export function CapabilityBanner() {
  const { isSecureContext, hasWebSerial, hasGamepadApi, setCapabilities } = useAppStore();

  useEffect(() => {
    setCapabilities({
      isSecureContext: window.isSecureContext,
      hasWebSerial: 'serial' in navigator,
      hasGamepadApi: 'getGamepads' in navigator,
    });
  }, [setCapabilities]);

  const allSupported = isSecureContext && hasWebSerial && hasGamepadApi;

  if (allSupported) {
    return (
      <div className="bg-green-50/50 border-b border-green-100 p-3 text-sm text-green-800 flex items-center justify-center gap-2">
        <CheckCircle2 className="w-4 h-4" />
        <span>Environment fully supported: Web Serial and Gamepad validation flows are available. Device config write/persist is serial-first (BLE config transport is not supported in current firmware).</span>
      </div>
    );
  }

  return (
    <div className="bg-amber-50/50 border-b border-amber-100 p-3 text-sm text-amber-800 flex flex-col items-center justify-center gap-1">
      <div className="flex items-center gap-2 font-medium">
        <AlertTriangle className="w-4 h-4" />
        <span>Environment Limitations Detected</span>
      </div>
      <div className="flex gap-4 text-xs opacity-90">
        {!isSecureContext && <span>• Serve over HTTPS or localhost before using Web Serial features.</span>}
        {!hasWebSerial && <span>• Use a Chromium-based desktop browser for Web Serial support.</span>}
        {!hasGamepadApi && <span>• Gamepad API unavailable: Validate view partially disabled.</span>}
      </div>
      <div className="text-xs opacity-80 mt-1">
        Device config write/persist is serial-first; BLE config transport is not supported in the current firmware.
      </div>
    </div>
  );
}
