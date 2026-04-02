'use client';

import { Shield, Chrome, Usb, RefreshCcw, Database } from 'lucide-react';

export function HelpView() {
  return (
    <div className="p-6 max-w-4xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight">Help & Troubleshooting</h2>
        <p className="text-gray-500 mt-1">Guidance for using the Charm Web Companion.</p>
      </div>

      <div className="grid gap-6 md:grid-cols-2">
        <div className="space-y-4">
          <div className="flex gap-4">
            <div className="mt-1 shrink-0 text-blue-600"><Shield className="w-5 h-5" /></div>
            <div>
              <h3 className="font-medium">Secure Context Required</h3>
              <p className="text-sm text-gray-600 mt-1">Web Serial features require a secure context. You must serve this application over HTTPS or localhost.</p>
            </div>
          </div>

          <div className="flex gap-4">
            <div className="mt-1 shrink-0 text-blue-600"><Chrome className="w-5 h-5" /></div>
            <div>
              <h3 className="font-medium">Supported Browsers</h3>
              <p className="text-sm text-gray-600 mt-1">The Web Serial API is currently only supported on Chromium-based desktop browsers (Chrome, Edge, Opera, Brave).</p>
            </div>
          </div>

          <div className="flex gap-4">
            <div className="mt-1 shrink-0 text-blue-600"><Usb className="w-5 h-5" /></div>
            <div>
              <h3 className="font-medium">Serial Ownership</h3>
              <p className="text-sm text-gray-600 mt-1">Only one tool can use the serial port at a time. You cannot flash firmware and monitor the console simultaneously. The app enforces this mutual exclusion.</p>
            </div>
          </div>
        </div>

        <div className="space-y-4">
          <div className="flex gap-4">
            <div className="mt-1 shrink-0 text-amber-600"><RefreshCcw className="w-5 h-5" /></div>
            <div>
              <h3 className="font-medium">Flashing Recovery</h3>
              <p className="text-sm text-gray-600 mt-1">
                If flashing fails:
                <ul className="list-disc pl-4 mt-2 space-y-1">
                  <li>Close other serial apps or browser tabs holding the port.</li>
                  <li>Reconnect the device and retry.</li>
                  <li>If bootloader sync fails, you may need to manually hold the BOOT button while pressing EN/RST.</li>
                </ul>
              </p>
            </div>
          </div>

          <div className="flex gap-4">
            <div className="mt-1 shrink-0 text-purple-600"><Database className="w-5 h-5" /></div>
            <div>
              <h3 className="font-medium">Config Transport Truth</h3>
              <p className="text-sm text-gray-600 mt-1">
                The device only stores a reference to your mapping bundle (ID, version, integrity) and profile ID. It does not store the full JSON draft. Your full drafts are saved locally in your browser.
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
