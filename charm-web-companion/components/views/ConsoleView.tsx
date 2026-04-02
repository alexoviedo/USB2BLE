'use client';

import { useState, useRef, useEffect } from 'react';
import { useAppStore } from '@/lib/store';
import { AlertCircle, Terminal, Trash2, Play, Square, Loader2 } from 'lucide-react';
import { WebSerialMonitor } from '@/lib/adapters/SerialMonitor';

export function ConsoleView() {
  const { serialOwner, setSerialOwner, setSerialPermissionState, hasWebSerial, isSecureContext } = useAppStore();
  const canUseConsole = hasWebSerial && isSecureContext;

  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [logs, setLogs] = useState<string>('');
  const [autoScroll, setAutoScroll] = useState(true);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [rxStats, setRxStats] = useState({ bytes: 0, chunks: 0, reading: false });
  const [portLabel, setPortLabel] = useState<string>('unknown');

  const monitorRef = useRef<WebSerialMonitor | null>(null);
  const logsEndRef = useRef<HTMLDivElement>(null);
  const viewportRef = useRef<HTMLDivElement>(null);

  // Initialize monitor once
  useEffect(() => {
    monitorRef.current = new WebSerialMonitor();
    
    monitorRef.current.onData((data) => {
      const stats = monitorRef.current?.getDiagnostics();
      if (stats) {
        setRxStats({ bytes: stats.bytesReceived, chunks: stats.chunksReceived, reading: stats.isReading });
      }
      setLogs((prev) => {
        const newLogs = prev + data;
        // Keep last 50,000 characters to prevent memory issues
        if (newLogs.length > 50000) {
          return newLogs.slice(-50000);
        }
        return newLogs;
      });
    });

    monitorRef.current.onError((error) => {
      setErrorMsg(error.message);
      setRxStats((prev) => ({ ...prev, reading: false }));
      handleDisconnect(false); // Force disconnect on error (e.g., unplug)
    });

    return () => {
      if (monitorRef.current) {
        monitorRef.current.disconnect().catch(() => {});
      }
      setSerialOwner('none');
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Auto-scroll logic
  useEffect(() => {
    if (autoScroll && logsEndRef.current && viewportRef.current) {
      viewportRef.current.scrollTop = viewportRef.current.scrollHeight;
    }
  }, [logs, autoScroll]);

  const handleConnect = async () => {
    if (!monitorRef.current) return;
    
    setIsConnecting(true);
    setErrorMsg(null);
    setSerialPermissionState('requesting_permission');
    
    try {
      await monitorRef.current.connect(115200);
      const stats = monitorRef.current.getDiagnostics();
      setRxStats({ bytes: stats.bytesReceived, chunks: stats.chunksReceived, reading: stats.isReading });
      const label = stats.portInfo
        ? `VID:0x${(stats.portInfo.usbVendorId ?? 0).toString(16).padStart(4, '0')} PID:0x${(stats.portInfo.usbProductId ?? 0).toString(16).padStart(4, '0')}`
        : 'unknown';
      setPortLabel(label);
      setSerialOwner('console');
      setSerialPermissionState('permission_granted');
      setIsConnected(true);
    } catch (err: any) {
      setSerialOwner('none');
      if (err.code === 'PORT_NOT_SELECTED') {
        setSerialPermissionState('request_cancelled');
      } else if (err.code === 'PERMISSION_DENIED') {
        setSerialPermissionState('permission_denied');
        setErrorMsg('Permission denied to access the serial port.');
      } else if (err.code === 'PORT_BUSY') {
        setSerialPermissionState('port_busy');
        setErrorMsg('Serial port is busy. Close other tabs or applications using it.');
      } else {
        setSerialPermissionState('unknown');
        setErrorMsg(err.message || 'Failed to connect to serial port.');
      }
    } finally {
      setIsConnecting(false);
    }
  };

  const handleDisconnect = async (intentional = true) => {
    if (!monitorRef.current) return;
    
    try {
      await monitorRef.current.disconnect();
    } catch (err) {
      console.error('Error during disconnect', err);
    } finally {
      setIsConnected(false);
      setSerialOwner('none');
      setRxStats({ bytes: 0, chunks: 0, reading: false });
      if (intentional) {
        setErrorMsg(null);
      }
    }
  };

  const handleClear = () => {
    setLogs('');
  };

  const isPortBusyByFlash = serialOwner === 'flash';
  const isPortBusyByConfig = serialOwner === 'config'; // Future proofing if config claims it
  const isBlocked = isPortBusyByFlash || isPortBusyByConfig;

  return (
    <div className="p-6 max-w-5xl mx-auto h-[calc(100vh-8rem)] flex flex-col">
      <div className="mb-6 flex items-center justify-between shrink-0">
        <div>
          <h2 className="text-2xl font-semibold tracking-tight">Serial Console</h2>
          <p className="text-gray-500 mt-1">Monitor device logs and debug output at 115200 baud.</p>
        </div>
        <div className="flex items-center gap-2">
          <button 
            onClick={handleClear}
            disabled={logs.length === 0}
            className="flex items-center gap-2 px-3 py-1.5 text-sm font-medium border rounded-md hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
          >
            <Trash2 className="w-4 h-4" /> Clear
          </button>
          
          {isConnected ? (
            <button 
              onClick={() => handleDisconnect(true)}
              className="flex items-center gap-2 px-3 py-1.5 text-sm font-medium bg-red-600 text-white rounded-md hover:bg-red-700 transition-colors"
            >
              <Square className="w-4 h-4" /> Disconnect
            </button>
          ) : (
            <button 
              onClick={handleConnect}
              disabled={!canUseConsole || isBlocked || isConnecting}
              className="flex items-center gap-2 px-3 py-1.5 text-sm font-medium bg-blue-600 text-white rounded-md hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
            >
              {isConnecting ? <Loader2 className="w-4 h-4 animate-spin" /> : <Play className="w-4 h-4" />}
              Connect
            </button>
          )}
        </div>
      </div>

      {!canUseConsole && (
        <div className="bg-red-50 text-red-800 p-4 rounded-lg flex items-start gap-3 border border-red-100 mb-6 shrink-0">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Console Unavailable</h3>
            <p className="text-sm mt-1">Your current environment does not support Web Serial. Please use a Chromium-based desktop browser and ensure you are on a secure context.</p>
          </div>
        </div>
      )}

      {isBlocked && !isConnected && (
        <div className="bg-amber-50 text-amber-800 p-4 rounded-lg flex items-start gap-3 border border-amber-100 mb-6 shrink-0">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Serial Port Busy</h3>
            <p className="text-sm mt-1">The serial port is currently owned by the {serialOwner === 'flash' ? 'Flasher' : 'Configuration tool'}. Please wait for the operation to complete before connecting the console.</p>
          </div>
        </div>
      )}

      {errorMsg && !isConnected && (
        <div className="bg-red-50 text-red-800 p-4 rounded-lg flex items-start gap-3 border border-red-100 mb-6 shrink-0">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Connection Error</h3>
            <p className="text-sm mt-1">{errorMsg}</p>
          </div>
        </div>
      )}

      <div 
        ref={viewportRef}
        className="flex-1 bg-gray-950 rounded-xl border shadow-inner p-4 font-mono text-sm text-gray-300 overflow-y-auto relative whitespace-pre-wrap break-all"
      >
        {!isConnected && logs.length === 0 && (
          <div className="absolute inset-0 flex items-center justify-center text-gray-600">
            <div className="flex flex-col items-center gap-2">
              <Terminal className="w-8 h-8" />
              <p>Not connected</p>
            </div>
          </div>
        )}
        {logs}
        <div ref={logsEndRef} />
      </div>
      
      <div className="mt-4 flex items-center justify-between text-xs text-gray-500 shrink-0">
        <div className="flex items-center gap-4">
          <span className="flex items-center gap-1.5">
            <span className={`w-2 h-2 rounded-full ${isConnected ? 'bg-green-500' : 'bg-gray-300'}`}></span> 
            {isConnected ? 'Connected' : 'Disconnected'}
          </span>
          <span>115200 baud</span>
          <span>port {portLabel}</span>
          <span>rx {rxStats.bytes} B / {rxStats.chunks} chunks</span>
        </div>
        <label className="flex items-center gap-2 cursor-pointer hover:text-gray-700 transition-colors">
          <input 
            type="checkbox" 
            checked={autoScroll}
            onChange={(e) => setAutoScroll(e.target.checked)}
            className="rounded border-gray-300 text-blue-600 focus:ring-blue-500" 
          />
          Auto-scroll
        </label>
      </div>
    </div>
  );
}
