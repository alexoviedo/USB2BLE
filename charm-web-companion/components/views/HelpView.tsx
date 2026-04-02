'use client';

import { Shield, Chrome, Usb, RefreshCcw, Database, HelpCircle } from 'lucide-react';

export function HelpView() {
  return (
    <div className="p-6 max-w-4xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight text-gray-900 flex items-center gap-2">
          <HelpCircle className="w-6 h-6 text-blue-600" />
          Help & Troubleshooting
        </h2>
        <p className="text-gray-500 mt-1">Guidance for using the Charm Web Companion.</p>
      </div>

      <div className="grid gap-8 md:grid-cols-2">
        {/* Environment Column */}
        <div className="space-y-6">
          <section className="bg-white p-5 rounded-2xl border shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-blue-600">
              <Shield className="w-5 h-5" />
              <h3 className="font-semibold text-gray-900 text-sm">Secure Context Required</h3>
            </div>
            <p className="text-xs text-gray-600 leading-relaxed">
              Web Serial features require a secure context. You must serve this application over <strong>HTTPS</strong> or <strong>localhost</strong>. Insecure environments will have serial features disabled.
            </p>
          </section>

          <section className="bg-white p-5 rounded-2xl border shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-blue-600">
              <Chrome className="w-5 h-5" />
              <h3 className="font-semibold text-gray-900 text-sm">Supported Browsers</h3>
            </div>
            <p className="text-xs text-gray-600 leading-relaxed">
              The Web Serial API is currently supported on <strong>Chromium-based desktop browsers</strong> (Chrome, Edge, Opera, Brave). Mobile browsers and non-Chromium browsers (Firefox, Safari) are not supported for flashing or console features.
            </p>
          </section>

          <section className="bg-white p-5 rounded-2xl border shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-blue-600">
              <Usb className="w-5 h-5" />
              <h3 className="font-semibold text-gray-900 text-sm">Serial Ownership</h3>
            </div>
            <div className="text-xs text-gray-600 leading-relaxed">
              Only one owner can use the serial port at a time. The app enforces this mutual exclusion:
              <ul className="mt-2 space-y-1 list-disc list-inside opacity-80">
                <li><strong>Flash</strong> owns the port during firmware updates.</li>
                <li><strong>Console</strong> owns the port during monitoring.</li>
                <li><strong>Config</strong> owns the port during command sync.</li>
              </ul>
              <p className="mt-2">You must disconnect one tool before using another.</p>
            </div>
          </section>
        </div>

        {/* Recovery Column */}
        <div className="space-y-6">
          <section className="bg-amber-50 p-5 rounded-2xl border border-amber-100 shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-amber-600">
              <RefreshCcw className="w-5 h-5" />
              <h3 className="font-semibold text-amber-900 text-sm">Flashing Recovery</h3>
            </div>
            <div className="text-xs text-amber-800 leading-relaxed space-y-2">
              <p>If flashing fails:</p>
              <ol className="list-decimal list-inside space-y-1 opacity-90">
                <li>Close other serial apps or browser tabs holding the port.</li>
                <li>Unplug and reconnect the device.</li>
                <li>If bootloader sync fails, manually hold the <strong>BOOT</strong> button while pressing/toggling <strong>EN/RST</strong>.</li>
              </ol>
            </div>
          </section>

          <section className="bg-purple-50 p-5 rounded-2xl border border-purple-100 shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-purple-600">
              <Database className="w-5 h-5" />
              <h3 className="font-semibold text-purple-900 text-sm">Config Transport Limitations</h3>
            </div>
            <p className="text-xs text-purple-800 leading-relaxed">
              The firmware’s current persist/load contract is <strong>NOT</strong> a full arbitrary config upload pipeline.
              The device only stores a reference to your mapping bundle (ID, version, integrity) and profile ID.
              <strong>Always export your rich JSON drafts</strong> to your local computer if you want to preserve the full layout for future editing.
            </p>
          </section>

          <section className="bg-gray-50 p-5 rounded-2xl border border-gray-200 shadow-sm space-y-3">
            <div className="flex items-center gap-3 text-gray-600">
              <Database className="w-5 h-5" />
              <h3 className="font-semibold text-gray-900 text-sm">Same-Site Artifacts</h3>
            </div>
            <div className="text-xs text-gray-600 leading-relaxed">
              In "Same-site manifest" mode, the app expects to find firmware artifacts relative to its deployment:
              <ul className="mt-2 space-y-1 list-disc list-inside opacity-80">
                <li>./firmware/manifest.json</li>
                <li>./firmware/bootloader.bin</li>
                <li>./firmware/partition-table.bin</li>
                <li>./firmware/charm.bin</li>
              </ul>
            </div>
          </section>
        </div>
      </div>
    </div>
  );
}
