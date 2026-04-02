'use client';

import { useState, useRef } from 'react';
import { useAppStore } from '@/lib/store';
import { AlertCircle, CheckCircle2, Loader2, UploadCloud, FileJson, Cpu } from 'lucide-react';
import { BrowserArtifactIngestion } from '@/lib/adapters/ArtifactIngestion';
import { WebSerialFlasher } from '@/lib/adapters/Flasher';
import { Manifest } from '@/lib/schema';
import { FlashError } from '@/lib/types';
import { logSerialLifecycleEvent } from '@/lib/serialLifecycle';

type SourceMode = 'same-site' | 'manual';
type FlashState = 'idle' | 'loading_artifacts' | 'artifacts_ready' | 'connecting' | 'flashing' | 'success' | 'error';

export function FlashView() {
  const { serialOwner, setSerialOwner, hasWebSerial, isSecureContext } = useAppStore();
  const canFlash = hasWebSerial && isSecureContext;

  const [sourceMode, setSourceMode] = useState<SourceMode>('same-site');
  const [flashState, setFlashState] = useState<FlashState>('idle');
  const [progress, setProgress] = useState(0);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  
  const [manifest, setManifest] = useState<Manifest | null>(null);
  const [deviceInfo, setDeviceInfo] = useState<{ chip: string; mac: string } | null>(null);
  const [isCoolingDown, setIsCoolingDown] = useState(false);

  const fileInputRef = useRef<HTMLInputElement>(null);
  const manualFilesRef = useRef<File[]>([]);

  const artifactsRef = useRef<{
    bootloader: ArrayBuffer;
    partitionTable: ArrayBuffer;
    app: ArrayBuffer;
  } | null>(null);

  const flasherRef = useRef(new WebSerialFlasher());
  const ingestionRef = useRef(new BrowserArtifactIngestion());

  const handleSourceChange = (mode: SourceMode) => {
    setSourceMode(mode);
    setFlashState('idle');
    setErrorMsg(null);
    setManifest(null);
    artifactsRef.current = null;
    manualFilesRef.current = [];
    if (fileInputRef.current) fileInputRef.current.value = '';
  };

  const handleManualFiles = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []);
    if (files.length === 0) return;
    
    manualFilesRef.current = files;
    await loadArtifacts();
  };

  const loadArtifacts = async () => {
    setFlashState('loading_artifacts');
    setErrorMsg(null);
    
    try {
      let loadedManifest: Manifest;
      let bootloader: ArrayBuffer;
      let partitionTable: ArrayBuffer;
      let app: ArrayBuffer;

      if (sourceMode === 'same-site') {
        loadedManifest = await ingestionRef.current.fetchSameSiteManifest();
        bootloader = await ingestionRef.current.fetchSameSiteBinary(loadedManifest.files.bootloader);
        partitionTable = await ingestionRef.current.fetchSameSiteBinary(loadedManifest.files.partition_table);
        app = await ingestionRef.current.fetchSameSiteBinary(loadedManifest.files.app);
      } else {
        const manifestFile = manualFilesRef.current.find(f => f.name === 'manifest.json');
        if (!manifestFile) throw new FlashError('manifest.json is missing from selected files', 'MISSING_ARTIFACTS');
        
        loadedManifest = await ingestionRef.current.parseManualManifest(manifestFile);
        
        const getFile = (name: string) => {
          const f = manualFilesRef.current.find(file => file.name === name);
          if (!f) throw new FlashError(`Required binary ${name} is missing`, 'MISSING_ARTIFACTS');
          return f;
        };

        bootloader = await ingestionRef.current.readManualBinary(getFile(loadedManifest.files.bootloader));
        partitionTable = await ingestionRef.current.readManualBinary(getFile(loadedManifest.files.partition_table));
        app = await ingestionRef.current.readManualBinary(getFile(loadedManifest.files.app));
      }

      setManifest(loadedManifest);
      artifactsRef.current = { bootloader, partitionTable, app };
      setFlashState('artifacts_ready');
    } catch (err: any) {
      setErrorMsg(err.message || 'Failed to load artifacts');
      setFlashState('error');
    }
  };

  const startFlash = async () => {
    if (!artifactsRef.current) return;
    
    setFlashState('connecting');
    setErrorMsg(null);
    setProgress(0);
    setDeviceInfo(null);

    // Claim serial ownership
    logSerialLifecycleEvent('flash', 'owner_acquire_requested', { currentOwner: serialOwner });
    setSerialOwner('flash');

    try {
      await flasherRef.current.connect();
      logSerialLifecycleEvent('flash', 'owner_acquire_succeeded', {});
      
      const chip = await flasherRef.current.getChipName() || 'Unknown ESP32';
      const mac = await flasherRef.current.getMacAddress() || 'Unknown MAC';
      setDeviceInfo({ chip, mac });

      setFlashState('flashing');
      
      await flasherRef.current.flash(
        artifactsRef.current.bootloader,
        artifactsRef.current.partitionTable,
        artifactsRef.current.app,
        (p) => setProgress(p)
      );

      setFlashState('success');
      setIsCoolingDown(true);
      setTimeout(() => setIsCoolingDown(false), 2000);
    } catch (err: any) {
      logSerialLifecycleEvent('flash', 'owner_acquire_failed', { message: err?.message ?? 'flash_failed' });
      setErrorMsg(err.message || 'Flash failed');
      setFlashState('error');
    } finally {
      await flasherRef.current.disconnect();
      // Release serial ownership to allow console usage
      setSerialOwner('none');
      logSerialLifecycleEvent('flash', 'owner_release_succeeded', {});
    }
  };

  const isBusy = flashState === 'loading_artifacts' || flashState === 'connecting' || flashState === 'flashing';
  const isPortBusy = serialOwner === 'console' || serialOwner === 'config';

  return (
    <div className="p-6 max-w-4xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight">Flash Firmware</h2>
        <p className="text-gray-500 mt-1">Flash the latest Charm firmware to your ESP32-S3 device.</p>
      </div>

      {!canFlash && (
        <div className="bg-red-50 text-red-800 p-4 rounded-lg flex items-start gap-3 border border-red-100">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Flashing Unavailable</h3>
            <p className="text-sm mt-1">Your current environment does not support Web Serial. Please use a Chromium-based desktop browser and ensure you are on a secure context (HTTPS or localhost).</p>
          </div>
        </div>
      )}

      {isPortBusy && (
        <div className="bg-amber-50 text-amber-800 p-4 rounded-lg flex items-start gap-3 border border-amber-100">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Serial Port Busy</h3>
            <p className="text-sm mt-1">The serial port is currently owned by the {serialOwner === 'console' ? 'Console' : 'Configuration tool'}. Please disconnect it before flashing.</p>
          </div>
        </div>
      )}

      {flashState === 'error' && errorMsg && (
        <div className="bg-red-50 text-red-800 p-4 rounded-lg flex items-start gap-3 border border-red-100">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Error</h3>
            <p className="text-sm mt-1">{errorMsg}</p>
          </div>
        </div>
      )}

      {flashState === 'success' && (
        <div className="bg-green-50 text-green-800 p-4 rounded-lg flex items-start gap-3 border border-green-100">
          <CheckCircle2 className="w-5 h-5 mt-0.5 shrink-0" />
          <div className="flex-1">
            <h3 className="font-medium">Flash Successful</h3>
            <p className="text-sm mt-1">
              {isCoolingDown
                ? 'The device has been flashed and is now resetting. Please wait a moment...'
                : 'The device has been flashed and reset. You can now switch to the Console view to monitor logs.'}
            </p>
          </div>
          <button 
            onClick={() => {
              window.dispatchEvent(new CustomEvent('navigate-tab', { detail: 'console' }));
            }}
            disabled={isCoolingDown}
            className="px-3 py-1.5 bg-green-600 text-white text-sm font-medium rounded-md hover:bg-green-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors flex items-center gap-2"
          >
            {isCoolingDown && <Loader2 className="w-4 h-4 animate-spin" />}
            Go to Console
          </button>
        </div>
      )}

      <div className="grid gap-6 md:grid-cols-2">
        <div className="border rounded-xl p-5 bg-white shadow-sm flex flex-col">
          <h3 className="font-medium mb-4">1. Select Artifacts</h3>
          <div className="space-y-4 flex-1">
            <div className={`flex items-center gap-3 p-3 border rounded-lg cursor-pointer transition-colors ${sourceMode === 'same-site' ? 'bg-blue-50 border-blue-200' : 'bg-gray-50 hover:bg-gray-100'}`} onClick={() => !isBusy && handleSourceChange('same-site')}>
              <input type="radio" id="same-site" name="artifact-source" checked={sourceMode === 'same-site'} readOnly disabled={isBusy} />
              <label htmlFor="same-site" className="text-sm font-medium cursor-pointer">Same-site manifest (./firmware/)</label>
            </div>
            <div className={`flex items-center gap-3 p-3 border rounded-lg cursor-pointer transition-colors ${sourceMode === 'manual' ? 'bg-blue-50 border-blue-200' : 'bg-gray-50 hover:bg-gray-100'}`} onClick={() => !isBusy && handleSourceChange('manual')}>
              <input type="radio" id="manual" name="artifact-source" checked={sourceMode === 'manual'} readOnly disabled={isBusy} />
              <label htmlFor="manual" className="text-sm font-medium cursor-pointer">Manual local import</label>
            </div>

            {sourceMode === 'manual' && (
              <div className="mt-4">
                <input 
                  type="file" 
                  multiple 
                  ref={fileInputRef}
                  onChange={handleManualFiles}
                  disabled={isBusy}
                  className="block w-full text-sm text-gray-500 file:mr-4 file:py-2 file:px-4 file:rounded-md file:border-0 file:text-sm file:font-semibold file:bg-blue-50 file:text-blue-700 hover:file:bg-blue-100"
                />
                <p className="text-xs text-gray-500 mt-2">Select manifest.json and all required .bin files together.</p>
              </div>
            )}

            {sourceMode === 'same-site' && flashState === 'idle' && (
              <button 
                onClick={loadArtifacts}
                disabled={isBusy}
                className="w-full py-2 px-4 bg-gray-900 text-white rounded-lg text-sm font-medium hover:bg-gray-800 disabled:opacity-50"
              >
                Load Same-Site Artifacts
              </button>
            )}

            {manifest && (
              <div className="mt-4 p-3 bg-gray-50 rounded-lg border text-xs space-y-2">
                <div className="flex items-center gap-2 text-gray-700 font-medium pb-2 border-b">
                  <FileJson className="w-4 h-4" /> Loaded Manifest
                </div>
                <div className="grid grid-cols-2 gap-2">
                  <span className="text-gray-500">Version:</span>
                  <span className="font-mono">{manifest.version}</span>
                  <span className="text-gray-500">Target:</span>
                  <span className="font-mono">{manifest.target}</span>
                  <span className="text-gray-500">Commit:</span>
                  <span className="font-mono truncate" title={manifest.commit_sha}>{manifest.commit_sha.substring(0, 7)}</span>
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="border rounded-xl p-5 bg-white shadow-sm flex flex-col">
          <h3 className="font-medium mb-4">2. Flash Device</h3>
          <div className="space-y-4 flex-1 flex flex-col">
            
            {deviceInfo && (
              <div className="mb-4 p-3 bg-blue-50/50 rounded-lg border border-blue-100 text-xs space-y-2">
                <div className="flex items-center gap-2 text-blue-900 font-medium pb-2 border-b border-blue-100">
                  <Cpu className="w-4 h-4" /> Device Identified
                </div>
                <div className="grid grid-cols-2 gap-2 text-blue-800">
                  <span className="opacity-70">Chip:</span>
                  <span className="font-mono">{deviceInfo.chip}</span>
                  <span className="opacity-70">MAC:</span>
                  <span className="font-mono">{deviceInfo.mac}</span>
                </div>
              </div>
            )}

            <div className="flex-1 flex flex-col justify-center">
              {flashState === 'flashing' && (
                <div className="space-y-2 mb-6">
                  <div className="flex justify-between text-sm font-medium">
                    <span>Flashing...</span>
                    <span>{progress}%</span>
                  </div>
                  <div className="w-full bg-gray-200 rounded-full h-2.5 overflow-hidden">
                    <div className="bg-blue-600 h-2.5 rounded-full transition-all duration-300" style={{ width: `${progress}%` }}></div>
                  </div>
                </div>
              )}

              <button 
                onClick={startFlash}
                disabled={!canFlash || isPortBusy || flashState !== 'artifacts_ready'}
                className="w-full py-3 px-4 bg-blue-600 text-white rounded-lg font-medium hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center gap-2 transition-colors"
              >
                {flashState === 'connecting' ? (
                  <><Loader2 className="w-5 h-5 animate-spin" /> Connecting...</>
                ) : flashState === 'flashing' ? (
                  <><Loader2 className="w-5 h-5 animate-spin" /> Flashing...</>
                ) : (
                  <><UploadCloud className="w-5 h-5" /> Connect & Flash</>
                )}
              </button>
              <div className="text-xs text-gray-500 text-center mt-3">
                Requires serial permission. Flashing will reset the device.
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
