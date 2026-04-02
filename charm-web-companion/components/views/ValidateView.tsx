'use client';

import { useAppStore } from '@/lib/store';
import { Gamepad2, AlertCircle } from 'lucide-react';

export function ValidateView() {
  const { hasGamepadApi } = useAppStore();

  return (
    <div className="p-6 max-w-4xl mx-auto space-y-8">
      <div>
        <h2 className="text-2xl font-semibold tracking-tight">Validate Inputs</h2>
        <p className="text-gray-500 mt-1">Browser-side validation using the Gamepad API.</p>
      </div>

      {!hasGamepadApi && (
        <div className="bg-red-50 text-red-800 p-4 rounded-lg flex items-start gap-3 border border-red-100">
          <AlertCircle className="w-5 h-5 mt-0.5 shrink-0" />
          <div>
            <h3 className="font-medium">Gamepad API Unavailable</h3>
            <p className="text-sm mt-1">Your browser does not support the Gamepad API. This view is disabled.</p>
          </div>
        </div>
      )}

      <div className="bg-white border rounded-xl shadow-sm overflow-hidden">
        <div className="p-12 flex flex-col items-center justify-center text-center space-y-4">
          <div className="w-16 h-16 bg-gray-100 rounded-full flex items-center justify-center text-gray-400">
            <Gamepad2 className="w-8 h-8" />
          </div>
          <div>
            <h3 className="text-lg font-medium text-gray-900">Waiting for Controller</h3>
            <p className="text-gray-500 mt-1 max-w-sm mx-auto">
              Press any button on your controller to make it visible to the browser.
            </p>
          </div>
          <div className="text-xs text-gray-400 max-w-md mx-auto mt-4">
            Note: This view shows raw browser Gamepad API data. It is not a BLE protocol inspector or proof of internal firmware state. If your controller is not visible, ensure it is paired at the OS level.
          </div>
        </div>
      </div>
    </div>
  );
}
