'use client';

import { useState, useEffect } from 'react';
import { CapabilityBanner } from '@/components/CapabilityBanner';
import { FlashView } from '@/components/views/FlashView';
import { ConsoleView } from '@/components/views/ConsoleView';
import { ConfigView } from '@/components/views/ConfigView';
import { ValidateView } from '@/components/views/ValidateView';
import { HelpView } from '@/components/views/HelpView';
import { Cpu, Terminal, Settings2, Gamepad2, HelpCircle } from 'lucide-react';

type Tab = 'flash' | 'console' | 'config' | 'validate' | 'help';

export default function Home() {
  const [activeTab, setActiveTab] = useState<Tab>('flash');

  useEffect(() => {
    const handleNavigate = (e: Event) => {
      const customEvent = e as CustomEvent<Tab>;
      if (customEvent.detail) {
        setActiveTab(customEvent.detail);
      }
    };
    window.addEventListener('navigate-tab', handleNavigate);
    return () => window.removeEventListener('navigate-tab', handleNavigate);
  }, []);

  const tabs = [
    { id: 'flash', label: 'Flash', icon: Cpu },
    { id: 'console', label: 'Console', icon: Terminal },
    { id: 'config', label: 'Config', icon: Settings2 },
    { id: 'validate', label: 'Validate', icon: Gamepad2 },
    { id: 'help', label: 'Help', icon: HelpCircle },
  ] as const;

  return (
    <div className="min-h-screen bg-gray-50 flex flex-col font-sans">
      <CapabilityBanner />
      
      <header className="bg-white border-b sticky top-0 z-10">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between h-16">
            <div className="flex items-center gap-2">
              <div className="w-8 h-8 bg-blue-600 rounded-lg flex items-center justify-center text-white font-bold">
                C
              </div>
              <span className="font-semibold text-gray-900 tracking-tight">Charm Companion</span>
            </div>
            
            <nav className="flex space-x-1">
              {tabs.map((tab) => {
                const Icon = tab.icon;
                const isActive = activeTab === tab.id;
                return (
                  <button
                    key={tab.id}
                    onClick={() => setActiveTab(tab.id)}
                    className={`flex items-center gap-2 px-3 py-2 text-sm font-medium rounded-md transition-colors ${
                      isActive 
                        ? 'bg-gray-100 text-gray-900' 
                        : 'text-gray-500 hover:text-gray-900 hover:bg-gray-50'
                    }`}
                  >
                    <Icon className="w-4 h-4" />
                    <span className="hidden sm:inline">{tab.label}</span>
                  </button>
                );
              })}
            </nav>
          </div>
        </div>
      </header>

      <main className="flex-1">
        {activeTab === 'flash' && <FlashView />}
        {activeTab === 'console' && <ConsoleView />}
        {activeTab === 'config' && <ConfigView />}
        {activeTab === 'validate' && <ValidateView />}
        {activeTab === 'help' && <HelpView />}
      </main>
    </div>
  );
}
