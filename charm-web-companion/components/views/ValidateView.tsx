'use client';

import { useState, useEffect, useRef } from 'react';
import { useAppStore } from '@/lib/store';
import { Gamepad2, AlertCircle, Info, Activity, Circle, Target } from 'lucide-react';
import { BrowserGamepad } from '@/lib/adapters/BrowserGamepad';

export function ValidateView() {
  const { hasGamepadApi } = useAppStore();
  const [gamepads, setGamepads] = useState<(Gamepad | null)[]>([]);
  const [activeIndex, setActiveIndex] = useState<number | null>(null);

  const gamepadAdapter = useRef(new BrowserGamepad());
  const rafId = useRef<number | null>(null);

  useEffect(() => {
    const updateGamepads = () => {
      setGamepads(gamepadAdapter.current.getGamepads());
      rafId.current = requestAnimationFrame(updateGamepads);
    };

    const handleConnected = (e: GamepadEvent) => {
      console.log('Gamepad connected:', e.gamepad);
      if (activeIndex === null) setActiveIndex(e.gamepad.index);
    };

    const handleDisconnected = (e: GamepadEvent) => {
      console.log('Gamepad disconnected:', e.gamepad);
      if (activeIndex === e.gamepad.index) setActiveIndex(null);
    };

    gamepadAdapter.current.onGamepadConnected(handleConnected);
    gamepadAdapter.current.onGamepadDisconnected(handleDisconnected);
    rafId.current = requestAnimationFrame(updateGamepads);

    return () => {
      if (rafId.current) cancelAnimationFrame(rafId.current);
      gamepadAdapter.current.removeListeners(handleConnected, handleDisconnected);
    };
  }, [activeIndex]);

  const activeGamepad = activeIndex !== null ? gamepads[activeIndex] : null;

  return (
    <div className="p-6 max-w-5xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight text-gray-900">Validate Inputs</h2>
        <p className="text-gray-500 mt-1">Browser-side validation using the Gamepad API.</p>
      </div>

      {!hasGamepadApi && (
        <div className="bg-red-50 text-red-800 p-4 rounded-xl flex items-start gap-3 border border-red-100 shadow-sm">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-semibold">Gamepad API Unavailable</h3>
            <p className="text-sm mt-1">Your browser does not support the Gamepad API. This view is disabled.</p>
          </div>
        </div>
      )}

      <div className="grid lg:grid-cols-3 gap-8">
        {/* Left Column: List & Troubleshooting */}
        <div className="space-y-6">
          <div className="bg-white border rounded-2xl shadow-sm overflow-hidden">
            <div className="border-b bg-gray-50/50 px-4 py-3 flex items-center gap-2">
              <Activity className="w-4 h-4 text-gray-400" />
              <h3 className="font-semibold text-sm">Controllers</h3>
            </div>
            <div className="p-2 space-y-1">
              {gamepads.filter(g => g !== null).length === 0 ? (
                <div className="p-4 text-center text-xs text-gray-400 italic">
                  No controllers detected
                </div>
              ) : (
                gamepads.map((gp, i) => gp && (
                  <button
                    key={i}
                    onClick={() => setActiveIndex(gp.index)}
                    className={`w-full text-left p-3 rounded-xl transition-all ${activeIndex === gp.index ? 'bg-blue-50 border-blue-100 border text-blue-900' : 'hover:bg-gray-50 text-gray-600'}`}
                  >
                    <div className="text-xs font-bold truncate">{gp.id}</div>
                    <div className="text-[10px] opacity-70 mt-0.5">Index: {gp.index} • {gp.buttons.length} buttons • {gp.axes.length} axes</div>
                  </button>
                ))
              )}
            </div>
          </div>

          <div className="bg-amber-50 border border-amber-100 rounded-2xl p-5 space-y-3">
            <h4 className="text-xs font-bold text-amber-900 uppercase tracking-widest flex items-center gap-2">
              <Info className="w-4 h-4" /> Troubleshooting
            </h4>
            <ul className="text-xs text-amber-800 space-y-2 leading-relaxed opacity-90">
              <li>• Pair/reconnect at OS level if controller is not visible.</li>
              <li>• <strong>Press any input</strong> if the browser has not surfaced the controller yet.</li>
              <li>• Browser focus or reconnect issues may affect visibility; ensure this tab is active.</li>
              <li className="pt-2 border-t border-amber-200/50 italic">
                Note: This is a raw HID monitor. It is not a BLE protocol inspector, a HID descriptor inspector, or proof of internal firmware BLE state.
              </li>
            </ul>
          </div>
        </div>

        {/* Right Column: Live Readout */}
        <div className="lg:col-span-2 space-y-6">
          {activeGamepad ? (
            <div className="bg-white border rounded-2xl shadow-sm overflow-hidden flex flex-col h-full">
              <div className="border-b bg-gray-50/50 px-6 py-4 flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></div>
                  <h3 className="font-semibold text-gray-900">Live Readout: {activeGamepad.id.split('(')[0]}</h3>
                </div>
                <span className="text-[10px] font-mono bg-gray-200 px-2 py-1 rounded">INDEX {activeGamepad.index}</span>
              </div>

              <div className="p-6 grid md:grid-cols-2 gap-8 overflow-y-auto">
                {/* Axes Section */}
                <div className="space-y-4">
                  <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest flex items-center gap-2">
                    <Target className="w-4 h-4" /> Axes
                  </h4>
                  <div className="space-y-4">
                    {activeGamepad.axes.map((val, i) => {
                      const isNeutral = Math.abs(val) < 0.1;
                      return (
                        <div key={i} className="space-y-1.5">
                          <div className="flex justify-between text-[10px] font-mono">
                            <span className="text-gray-500">AXIS {i}</span>
                            <span className={isNeutral ? 'text-gray-400' : 'text-blue-600 font-bold'}>{val.toFixed(4)}</span>
                          </div>
                          <div className="relative h-4 bg-gray-100 rounded-full overflow-hidden border">
                            <div className="absolute inset-y-0 left-1/2 w-px bg-gray-300 z-10"></div>
                            <div
                              className={`absolute inset-y-0 transition-all duration-75 ${isNeutral ? 'bg-gray-300' : 'bg-blue-500'}`}
                              style={{
                                left: val < 0 ? `${50 + val * 50}%` : '50%',
                                width: `${Math.abs(val) * 50}%`
                              }}
                            ></div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>

                {/* Buttons Section */}
                <div className="space-y-4">
                  <h4 className="text-xs font-bold text-gray-400 uppercase tracking-widest flex items-center gap-2">
                    <Circle className="w-4 h-4" /> Buttons
                  </h4>
                  <div className="grid grid-cols-4 sm:grid-cols-6 gap-2">
                    {activeGamepad.buttons.map((btn, i) => (
                      <div
                        key={i}
                        className={`flex flex-col items-center justify-center p-2 rounded-lg border transition-all ${btn.pressed ? 'bg-blue-600 border-blue-600 text-white shadow-md' : 'bg-gray-50 text-gray-400'}`}
                      >
                        <span className="text-[10px] font-bold">{i}</span>
                        <span className="text-[8px] opacity-70">{btn.value.toFixed(1)}</span>
                      </div>
                    ))}
                  </div>
                </div>
              </div>

              {/* Neutral Guidance */}
              <div className="mt-auto border-t p-4 bg-gray-50/50">
                <div className="flex items-center gap-3">
                  <div className="p-2 bg-white rounded-lg border shadow-sm">
                    <div className="w-3 h-3 rounded-full bg-gray-300"></div>
                  </div>
                  <div>
                    <h5 className="text-xs font-bold text-gray-700">Neutral/Center Guidance</h5>
                    <p className="text-[10px] text-gray-500 mt-0.5">Ensure sticks are centered. Gray bars indicate near-neutral values (&lt; 0.1). High values at rest may indicate drift.</p>
                  </div>
                </div>
              </div>
            </div>
          ) : (
            <div className="bg-white border border-dashed rounded-2xl h-full flex flex-col items-center justify-center p-12 text-center space-y-4">
              <div className="w-16 h-16 bg-gray-50 rounded-full flex items-center justify-center text-gray-300">
                <Gamepad2 className="w-8 h-8" />
              </div>
              <div>
                <h3 className="text-lg font-medium text-gray-900">No Controller Selected</h3>
                <p className="text-sm text-gray-500 max-w-xs mx-auto mt-1">
                  Select a detected controller from the list or press any button to wake it up.
                </p>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
