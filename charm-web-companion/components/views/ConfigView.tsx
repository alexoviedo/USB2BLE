'use client';

import { useState, useEffect, useRef } from 'react';
import { useAppStore } from '@/lib/store';
import {
  AlertCircle, Save, Upload, Download, RefreshCw,
  Trash2, Database, Laptop, Info, CheckCircle2, Loader2,
  Plus, X, ChevronDown, ChevronUp
} from 'lucide-react';
import { LocalDraft, LocalDraftSchema, MappingBundleRefSchema } from '@/lib/schema';
import { SerialConfigTransport } from '@/lib/adapters/SerialConfigTransport';
import { ConfigRequestEnvelope, ConfigResponseEnvelope } from '@/lib/schema';
import { logSerialLifecycleEvent } from '@/lib/serialLifecycle';

const DEFAULT_DRAFT: LocalDraft = {
  metadata: {
    name: "default-layout-v1",
    author: "local",
    revision: 1,
    notes: "",
    updatedAt: new Date().toISOString()
  },
  global: {
    scale: 1,
    deadzone: 0.08,
    clampMin: -1,
    clampMax: 1
  },
  axes: {
    "move_x": { index: 0, scale: 1, deadzone: 0.08, invert: false },
    "move_y": { index: 1, scale: 1, deadzone: 0.08, invert: false }
  },
  buttons: {
    "action_a": { index: 0 },
    "action_b": { index: 1 },
    "menu": { index: 9 }
  }
};

const STORAGE_KEY = 'charm_local_draft';

export function ConfigView() {
  const { serialOwner, setSerialOwner, hasWebSerial, isSecureContext } = useAppStore();
  const canUseSerial = hasWebSerial && isSecureContext;

  // Local Draft State
  const [draft, setDraft] = useState<LocalDraft>(DEFAULT_DRAFT);
  const [lastSavedDraft, setLastSavedDraft] = useState<string>(JSON.stringify(DEFAULT_DRAFT));
  const [validationErrors, setValidationErrors] = useState<string[]>([]);

  // Device Transport State
  const [isConnecting, setIsConnecting] = useState(false);
  const [deviceStatus, setDeviceStatus] = useState<string | null>(null);
  const [deviceError, setDeviceError] = useState<string | null>(null);
  const [capabilities, setCapabilities] = useState<any>(null);
  const [deviceConfig, setDeviceConfig] = useState<any>(null);

  // Device Command Inputs (separate from draft as per contract)
  const [mappingBundle, setMappingBundle] = useState({ bundle_id: 0, version: 0, integrity: 0 });
  const [profileId, setProfileId] = useState(0);
  const [bondingMaterial, setBondingMaterial] = useState<number[]>([]);

  const transportRef = useRef(new SerialConfigTransport());
  const nextRequestId = useRef(1);

  // Load from local storage on mount
  useEffect(() => {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      try {
        const parsed = JSON.parse(saved);
        const validated = LocalDraftSchema.parse(parsed);
        setDraft(validated);
        setLastSavedDraft(saved);
      } catch (e) {
        console.warn('Failed to load saved draft, using default', e);
      }
    }
  }, []);

  const hasUnsavedChanges = JSON.stringify(draft) !== lastSavedDraft;

  // --- Local Actions ---

  const handleUpdateDraft = (updater: (prev: LocalDraft) => LocalDraft) => {
    setDraft(prev => {
      const next = updater(prev);
      next.metadata.updatedAt = new Date().toISOString();
      return next;
    });
  };

  const validateLocal = () => {
    const result = LocalDraftSchema.safeParse(draft);
    if (result.success) {
      setValidationErrors([]);
      return true;
    } else {
      setValidationErrors(result.error.issues.map(e => `${e.path.join('.')}: ${e.message}`));
      return false;
    }
  };

  const saveLocally = () => {
    if (!validateLocal()) return;
    const json = JSON.stringify(draft);
    localStorage.setItem(STORAGE_KEY, json);
    setLastSavedDraft(json);
    setDeviceStatus('Draft saved to browser storage.');
  };

  const clearLocal = () => {
    localStorage.removeItem(STORAGE_KEY);
    setDraft(DEFAULT_DRAFT);
    setLastSavedDraft(JSON.stringify(DEFAULT_DRAFT));
    setDeviceStatus('Browser storage cleared.');
  };

  const exportJson = () => {
    const blob = new Blob([JSON.stringify(draft, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `charm-config-${draft.metadata.name}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const importJson = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (ev) => {
      try {
        const json = JSON.parse(ev.target?.result as string);
        const validated = LocalDraftSchema.parse(json);
        setDraft(validated);
        setDeviceStatus('Draft imported successfully.');
      } catch (err: any) {
        setDeviceError(`Import failed: ${err.message}`);
      }
    };
    reader.readAsText(file);
    e.target.value = '';
  };

  // --- Device Actions ---

  const runDeviceCommand = async (command: string, payload: any = {}) => {
    if (!canUseSerial) return;
    if (serialOwner !== 'none' && serialOwner !== 'config') {
      setDeviceError(`Serial port busy (${serialOwner})`);
      return;
    }

    setIsConnecting(true);
    setDeviceError(null);
    setDeviceStatus(`Executing ${command}...`);

    // Explicitly claim owner
    logSerialLifecycleEvent('config', 'owner_acquire_requested', { currentOwner: serialOwner, command });
    setSerialOwner('config');

    try {
      await transportRef.current.connect();
      logSerialLifecycleEvent('config', 'owner_acquire_succeeded', { command });

      const request: ConfigRequestEnvelope = {
        protocol_version: 1,
        request_id: nextRequestId.current++,
        command: command as any,
        payload,
        integrity: 'CFG1'
      };

      const response = await transportRef.current.sendCommand(request);

      if (response.status === 'kOk') {
        setDeviceStatus(`${command} successful.`);
        if (command === 'config.get_capabilities') setCapabilities(response.capabilities);
        if (command === 'config.load') {
          setDeviceConfig(response.payload);
          if (response.payload?.mapping_bundle) {
            setMappingBundle(response.payload.mapping_bundle);
          }
          if (response.payload?.profile_id) {
            setProfileId(response.payload.profile_id);
          }
        }
      } else {
        setDeviceError(`${command} failed: ${response.status} ${response.fault?.category || ''}`);
      }
    } catch (err: any) {
      logSerialLifecycleEvent('config', 'owner_acquire_failed', { command, message: err?.message ?? 'transport_error' });
      setDeviceError(err.message || 'Transport error');
    } finally {
      await transportRef.current.disconnect();
      setSerialOwner('none');
      logSerialLifecycleEvent('config', 'owner_release_succeeded', { command });
      setIsConnecting(false);
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto space-y-8">
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <h2 className="text-2xl font-semibold tracking-tight text-gray-900">Configuration</h2>
          <p className="text-gray-500 mt-1">Author local drafts and sync mapping bundles to the device.</p>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={saveLocally}
            disabled={!hasUnsavedChanges}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg text-sm font-medium hover:bg-blue-700 disabled:opacity-50 disabled:grayscale transition-all"
          >
            <Save className="w-4 h-4" /> Save Locally
          </button>
          <div className="h-8 w-px bg-gray-200 mx-1"></div>
          <button
            onClick={exportJson}
            className="p-2 text-gray-600 hover:text-gray-900 hover:bg-gray-100 rounded-lg transition-colors"
            title="Export JSON"
          >
            <Download className="w-5 h-5" />
          </button>
          <label className="p-2 text-gray-600 hover:text-gray-900 hover:bg-gray-100 rounded-lg transition-colors cursor-pointer" title="Import JSON">
            <Upload className="w-5 h-5" />
            <input type="file" className="hidden" accept=".json" onChange={importJson} />
          </label>
          <button
            onClick={clearLocal}
            className="p-2 text-gray-400 hover:text-red-600 hover:bg-red-50 rounded-lg transition-colors"
            title="Clear Draft"
          >
            <Trash2 className="w-5 h-5" />
          </button>
        </div>
      </div>

      <div className="bg-blue-50 text-blue-900 p-4 rounded-xl flex items-start gap-3 border border-blue-100 text-sm shadow-sm">
        <Info className="w-5 h-5 mt-0.5 shrink-0 text-blue-600" />
        <div>
          <h3 className="font-semibold text-blue-950">Truth Boundary</h3>
          <p className="mt-1 opacity-90 leading-relaxed">
            The rich draft editor below is for <strong>local planning</strong>. The ESP32-S3 firmware currently persists only a <strong>mapping bundle reference</strong> (ID, version, integrity) and <strong>profile ID</strong>. It does not store the full JSON draft.
          </p>
        </div>
      </div>

      <div className="grid lg:grid-cols-12 gap-8">
        {/* LEFT: Draft Editor */}
        <div className="lg:col-span-8 space-y-6">
          <div className="bg-white border rounded-2xl shadow-sm overflow-hidden">
            <div className="border-b bg-gray-50/50 px-6 py-4 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Laptop className="w-5 h-5 text-gray-400" />
                <h3 className="font-semibold text-gray-900">Local Draft Editor</h3>
              </div>
              {hasUnsavedChanges && (
                <span className="text-[10px] uppercase tracking-wider font-bold bg-amber-100 text-amber-700 px-2 py-0.5 rounded-full">
                  Unsaved Changes
                </span>
              )}
            </div>

            <div className="p-6 space-y-8">
              {/* Metadata Section */}
              <section className="space-y-4">
                <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest">Metadata</h4>
                <div className="grid sm:grid-cols-2 gap-4">
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Layout Name</label>
                    <input
                      type="text"
                      value={draft.metadata.name}
                      onChange={e => handleUpdateDraft(d => ({ ...d, metadata: { ...d.metadata, name: e.target.value } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Author</label>
                    <input
                      type="text"
                      value={draft.metadata.author}
                      onChange={e => handleUpdateDraft(d => ({ ...d, metadata: { ...d.metadata, author: e.target.value } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                </div>
              </section>

              {/* Global Section */}
              <section className="space-y-4">
                <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest">Global Response</h4>
                <div className="grid sm:grid-cols-4 gap-4">
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Scale</label>
                    <input
                      type="number" step="0.1"
                      value={draft.global.scale}
                      onChange={e => handleUpdateDraft(d => ({ ...d, global: { ...d.global, scale: parseFloat(e.target.value) } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Deadzone</label>
                    <input
                      type="number" step="0.01"
                      value={draft.global.deadzone}
                      onChange={e => handleUpdateDraft(d => ({ ...d, global: { ...d.global, deadzone: parseFloat(e.target.value) } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Clamp Min</label>
                    <input
                      type="number" step="0.1"
                      value={draft.global.clampMin}
                      onChange={e => handleUpdateDraft(d => ({ ...d, global: { ...d.global, clampMin: parseFloat(e.target.value) } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-sm font-medium text-gray-700">Clamp Max</label>
                    <input
                      type="number" step="0.1"
                      value={draft.global.clampMax}
                      onChange={e => handleUpdateDraft(d => ({ ...d, global: { ...d.global, clampMax: parseFloat(e.target.value) } }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm focus:ring-2 focus:ring-blue-500 outline-none"
                    />
                  </div>
                </div>
              </section>

              {/* Axes Section */}
              <section className="space-y-4">
                <div className="flex items-center justify-between">
                  <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest">Axes Mapping</h4>
                </div>
                <div className="grid gap-3">
                  {Object.entries(draft.axes).map(([key, axis]) => (
                    <div key={key} className="flex items-center gap-4 p-3 bg-gray-50 rounded-xl border group">
                      <div className="w-24 font-mono text-xs font-bold text-gray-500">{key}</div>
                      <div className="grid grid-cols-4 gap-3 flex-1">
                        <div className="flex flex-col">
                          <label className="text-[10px] text-gray-400 uppercase font-bold">Index</label>
                          <input type="number" value={axis.index} onChange={e => handleUpdateDraft(d => ({ ...d, axes: { ...d.axes, [key]: { ...axis, index: parseInt(e.target.value) } } }))} className="bg-transparent border-b border-gray-200 text-sm py-0.5 outline-none focus:border-blue-500" />
                        </div>
                        <div className="flex flex-col">
                          <label className="text-[10px] text-gray-400 uppercase font-bold">Scale</label>
                          <input type="number" step="0.1" value={axis.scale} onChange={e => handleUpdateDraft(d => ({ ...d, axes: { ...d.axes, [key]: { ...axis, scale: parseFloat(e.target.value) } } }))} className="bg-transparent border-b border-gray-200 text-sm py-0.5 outline-none focus:border-blue-500" />
                        </div>
                        <div className="flex flex-col">
                          <label className="text-[10px] text-gray-400 uppercase font-bold">Dead</label>
                          <input type="number" step="0.01" value={axis.deadzone} onChange={e => handleUpdateDraft(d => ({ ...d, axes: { ...d.axes, [key]: { ...axis, deadzone: parseFloat(e.target.value) } } }))} className="bg-transparent border-b border-gray-200 text-sm py-0.5 outline-none focus:border-blue-500" />
                        </div>
                        <div className="flex items-center gap-2">
                          <input type="checkbox" checked={axis.invert} onChange={e => handleUpdateDraft(d => ({ ...d, axes: { ...d.axes, [key]: { ...axis, invert: e.target.checked } } }))} className="rounded text-blue-600" />
                          <label className="text-[10px] text-gray-400 uppercase font-bold">Invert</label>
                        </div>
                      </div>
                      <button
                        onClick={() => handleUpdateDraft(d => { const n = { ...d.axes }; delete n[key]; return { ...d, axes: n }; })}
                        className="p-1 text-gray-300 hover:text-red-500 transition-colors"
                      >
                        <X className="w-4 h-4" />
                      </button>
                    </div>
                  ))}
                  <button
                    onClick={() => {
                      const name = prompt('Axis identifier (e.g. thumb_lx):');
                      if (name) handleUpdateDraft(d => ({ ...d, axes: { ...d.axes, [name]: { index: 0, scale: 1, deadzone: 0.08, invert: false } } }));
                    }}
                    className="flex items-center justify-center gap-2 py-2 border-2 border-dashed border-gray-200 rounded-xl text-sm text-gray-400 hover:border-blue-300 hover:text-blue-500 transition-all"
                  >
                    <Plus className="w-4 h-4" /> Add Axis Mapping
                  </button>
                </div>
              </section>

              {/* Buttons Section */}
              <section className="space-y-4">
                <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest">Button Mapping</h4>
                <div className="grid gap-3">
                  {Object.entries(draft.buttons).map(([key, btn]) => (
                    <div key={key} className="flex items-center gap-4 p-3 bg-gray-50 rounded-xl border">
                      <div className="w-24 font-mono text-xs font-bold text-gray-500">{key}</div>
                      <div className="flex flex-col flex-1">
                        <label className="text-[10px] text-gray-400 uppercase font-bold">Hardware Index</label>
                        <input type="number" value={btn.index} onChange={e => handleUpdateDraft(d => ({ ...d, buttons: { ...d.buttons, [key]: { index: parseInt(e.target.value) } } }))} className="bg-transparent border-b border-gray-200 text-sm py-0.5 outline-none focus:border-blue-500 w-20" />
                      </div>
                      <button
                         onClick={() => handleUpdateDraft(d => { const n = { ...d.buttons }; delete n[key]; return { ...d, buttons: n }; })}
                         className="p-1 text-gray-300 hover:text-red-500 transition-colors"
                      >
                        <X className="w-4 h-4" />
                      </button>
                    </div>
                  ))}
                  <button
                    onClick={() => {
                      const name = prompt('Button identifier (e.g. fire):');
                      if (name) handleUpdateDraft(d => ({ ...d, buttons: { ...d.buttons, [name]: { index: 0 } } }));
                    }}
                    className="flex items-center justify-center gap-2 py-2 border-2 border-dashed border-gray-200 rounded-xl text-sm text-gray-400 hover:border-blue-300 hover:text-blue-500 transition-all"
                  >
                    <Plus className="w-4 h-4" /> Add Button Mapping
                  </button>
                </div>
              </section>
            </div>
          </div>
        </div>

        {/* RIGHT: Device & Status */}
        <div className="lg:col-span-4 space-y-6">
          {/* Device Sync Card */}
          <div className="bg-white border rounded-2xl shadow-sm overflow-hidden">
            <div className="border-b bg-gray-50/50 px-6 py-4 flex items-center gap-2">
              <Database className="w-5 h-5 text-gray-400" />
              <h3 className="font-semibold text-gray-900">Device Sync</h3>
            </div>
            <div className="p-6 space-y-6">
              <div className="space-y-4">
                <div className="grid grid-cols-2 gap-4">
                  <div className="space-y-1.5">
                    <label className="text-[11px] font-bold text-gray-400 uppercase">Bundle ID</label>
                    <input
                      type="number"
                      value={mappingBundle.bundle_id}
                      onChange={e => setMappingBundle(prev => ({ ...prev, bundle_id: parseInt(e.target.value) }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm bg-gray-50 focus:bg-white transition-colors outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-[11px] font-bold text-gray-400 uppercase">Version</label>
                    <input
                      type="number"
                      value={mappingBundle.version}
                      onChange={e => setMappingBundle(prev => ({ ...prev, version: parseInt(e.target.value) }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm bg-gray-50 focus:bg-white transition-colors outline-none"
                    />
                  </div>
                </div>
                <div className="grid grid-cols-2 gap-4">
                  <div className="space-y-1.5">
                    <label className="text-[11px] font-bold text-gray-400 uppercase">Integrity</label>
                    <input
                      type="number"
                      value={mappingBundle.integrity}
                      onChange={e => setMappingBundle(prev => ({ ...prev, integrity: parseInt(e.target.value) }))}
                      className="w-full px-3 py-2 border rounded-lg text-sm bg-gray-50 focus:bg-white transition-colors outline-none"
                    />
                  </div>
                  <div className="space-y-1.5">
                    <label className="text-[11px] font-bold text-gray-400 uppercase">Profile ID</label>
                    <input
                      type="number"
                      value={profileId}
                      onChange={e => setProfileId(parseInt(e.target.value))}
                      className="w-full px-3 py-2 border rounded-lg text-sm bg-gray-50 focus:bg-white transition-colors outline-none"
                    />
                  </div>
                </div>
              </div>

              <div className="space-y-2 pt-4">
                <button
                  onClick={() => runDeviceCommand('config.persist', {
                    mapping_bundle: mappingBundle,
                    profile_id: profileId,
                    bonding_material: bondingMaterial.length > 0 ? bondingMaterial : undefined
                  })}
                  disabled={!canUseSerial || isConnecting || serialOwner === 'flash'}
                  className="w-full flex items-center justify-center gap-2 py-2.5 bg-gray-900 text-white rounded-xl text-sm font-semibold hover:bg-gray-800 disabled:opacity-50 transition-all shadow-sm"
                >
                  {isConnecting ? <Loader2 className="w-4 h-4 animate-spin" /> : <Save className="w-4 h-4" />}
                  Persist to Device
                </button>
                <div className="grid grid-cols-2 gap-2">
                  <button
                    onClick={() => runDeviceCommand('config.load')}
                    disabled={!canUseSerial || isConnecting || serialOwner === 'flash'}
                    className="flex items-center justify-center gap-2 py-2 border rounded-xl text-xs font-medium hover:bg-gray-50 disabled:opacity-50 transition-colors"
                  >
                    Load Current
                  </button>
                  <button
                    onClick={() => runDeviceCommand('config.get_capabilities')}
                    disabled={!canUseSerial || isConnecting || serialOwner === 'flash'}
                    className="flex items-center justify-center gap-2 py-2 border rounded-xl text-xs font-medium hover:bg-gray-50 disabled:opacity-50 transition-colors"
                  >
                    <RefreshCw className="w-3 h-3" /> Capabilities
                  </button>
                </div>
                <button
                  onClick={() => { if(confirm('Clear device configuration?')) runDeviceCommand('config.clear'); }}
                  disabled={!canUseSerial || isConnecting || serialOwner === 'flash'}
                  className="w-full py-2 text-red-600 text-xs font-medium hover:bg-red-50 rounded-xl transition-colors border border-transparent hover:border-red-100"
                >
                  Clear Device Config
                </button>
              </div>

              {!canUseSerial && (
                <p className="text-[10px] text-center text-red-500 font-medium">Serial access required for device sync.</p>
              )}
            </div>
          </div>

          {/* Operation Status */}
          <div className="bg-white border rounded-2xl shadow-sm p-5 space-y-4">
            <h4 className="text-[11px] font-bold text-gray-400 uppercase tracking-widest">Operation Status</h4>

            {deviceError && (
              <div className="p-3 bg-red-50 border border-red-100 rounded-xl flex items-start gap-2 animate-in fade-in slide-in-from-top-1">
                <AlertCircle className="w-4 h-4 text-red-600 mt-0.5 shrink-0" />
                <div className="text-xs text-red-800 font-medium">{deviceError}</div>
              </div>
            )}

            {deviceStatus && (
              <div className="p-3 bg-green-50 border border-green-100 rounded-xl flex items-start gap-2 animate-in fade-in slide-in-from-top-1">
                <CheckCircle2 className="w-4 h-4 text-green-600 mt-0.5 shrink-0" />
                <div className="text-xs text-green-800 font-medium">{deviceStatus}</div>
              </div>
            )}

            {!deviceError && !deviceStatus && (
              <div className="text-center py-4 text-gray-400 text-xs italic">
                No recent operations
              </div>
            )}

            {validationErrors.length > 0 && (
              <div className="pt-2 border-t">
                <p className="text-[10px] font-bold text-red-500 uppercase mb-2">Local Validation Errors</p>
                <ul className="space-y-1">
                  {validationErrors.map((err, idx) => (
                    <li key={idx} className="text-[10px] text-red-600 list-disc list-inside truncate">{err}</li>
                  ))}
                </ul>
              </div>
            )}
          </div>

          {/* Capabilities Info */}
          {capabilities && (
            <div className="bg-gray-900 rounded-2xl p-5 text-gray-300 space-y-3 shadow-xl">
              <h4 className="text-[10px] font-bold text-gray-500 uppercase tracking-widest">Device Capabilities</h4>
              <div className="grid grid-cols-2 gap-y-2 text-[11px] font-mono">
                <span className="text-gray-500">Protocol:</span> <span>v{capabilities.protocol_version}</span>
                <span className="text-gray-500">Persist:</span> <span className={capabilities.supports_persist ? 'text-green-400' : 'text-red-400'}>{capabilities.supports_persist ? 'YES' : 'NO'}</span>
                <span className="text-gray-500">Load:</span> <span className={capabilities.supports_load ? 'text-green-400' : 'text-red-400'}>{capabilities.supports_load ? 'YES' : 'NO'}</span>
                <span className="text-gray-500">BLE Transport:</span> <span className="text-amber-400">{capabilities.supports_ble_transport ? 'YES' : 'NO'}</span>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
