'use client';

import { useAppStore } from '@/lib/store';
import { AlertCircle, Save, Upload, Download, RefreshCw } from 'lucide-react';

export function ConfigView() {
  const { serialOwner, hasWebSerial, isSecureContext } = useAppStore();
  const canUseSerial = hasWebSerial && isSecureContext;

  return (
    <div className="p-6 max-w-5xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight">Configuration</h2>
        <p className="text-gray-500 mt-1">Author local drafts and sync mapping bundles to the device.</p>
      </div>

      <div className="bg-blue-50 text-blue-900 p-4 rounded-lg flex items-start gap-3 border border-blue-100 text-sm">
        <AlertCircle className="w-5 h-5 mt-0.5 shrink-0 text-blue-600" />
        <div>
          <h3 className="font-medium text-blue-950">Truth Boundary Notice</h3>
          <p className="mt-1 opacity-90">
            The local draft editor allows you to create rich mapping configurations. However, the device only persists a reference to the mapping bundle (ID, version, integrity) and profile ID. The full draft is saved locally in your browser.
          </p>
        </div>
      </div>

      <div className="grid lg:grid-cols-3 gap-6">
        <div className="lg:col-span-2 space-y-6">
          <div className="border rounded-xl bg-white shadow-sm overflow-hidden">
            <div className="border-b bg-gray-50 px-4 py-3 flex items-center justify-between">
              <h3 className="font-medium">Local Draft Editor</h3>
              <div className="flex items-center gap-2">
                <button className="p-1.5 text-gray-500 hover:text-gray-900 hover:bg-gray-200 rounded" title="Import JSON">
                  <Upload className="w-4 h-4" />
                </button>
                <button className="p-1.5 text-gray-500 hover:text-gray-900 hover:bg-gray-200 rounded" title="Export JSON">
                  <Download className="w-4 h-4" />
                </button>
              </div>
            </div>
            <div className="p-4">
              <div className="bg-gray-50 border rounded-lg p-8 text-center text-gray-500 text-sm">
                Draft editor UI placeholder
              </div>
            </div>
            <div className="border-t bg-gray-50 px-4 py-3 flex items-center justify-between text-sm">
              <span className="text-gray-500">Unsaved changes</span>
              <button disabled className="flex items-center gap-2 px-3 py-1.5 bg-gray-900 text-white rounded-md font-medium disabled:opacity-50">
                <Save className="w-4 h-4" /> Save Locally
              </button>
            </div>
          </div>
        </div>

        <div className="space-y-6">
          <div className="border rounded-xl bg-white shadow-sm overflow-hidden">
            <div className="border-b bg-gray-50 px-4 py-3">
              <h3 className="font-medium">Device Operations</h3>
            </div>
            <div className="p-4 space-y-4">
              {!canUseSerial && (
                <div className="text-xs text-red-600 bg-red-50 p-2 rounded border border-red-100">
                  Serial operations unavailable in this environment.
                </div>
              )}
              {serialOwner === 'flash' && (
                <div className="text-xs text-amber-600 bg-amber-50 p-2 rounded border border-amber-100">
                  Port busy (Flasher).
                </div>
              )}
              
              <div className="space-y-2">
                <button disabled className="w-full flex items-center justify-center gap-2 py-2 px-3 border rounded-md text-sm font-medium hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed">
                  <RefreshCw className="w-4 h-4" /> Get Capabilities
                </button>
                <button disabled className="w-full flex items-center justify-center gap-2 py-2 px-3 border rounded-md text-sm font-medium hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed">
                  Load from Device
                </button>
                <button disabled className="w-full flex items-center justify-center gap-2 py-2 px-3 bg-blue-600 text-white rounded-md text-sm font-medium hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed">
                  Persist to Device
                </button>
                <button disabled className="w-full flex items-center justify-center gap-2 py-2 px-3 border border-red-200 text-red-600 rounded-md text-sm font-medium hover:bg-red-50 disabled:opacity-50 disabled:cursor-not-allowed">
                  Clear Device Config
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
