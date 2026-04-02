import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import { HelpView } from '../components/views/HelpView';
import { ValidateView } from '../components/views/ValidateView';
import { FlashView } from '../components/views/FlashView';
import { ConsoleView } from '../components/views/ConsoleView';
import { useAppStore } from '../lib/store';
import React from 'react';

// Mock the store
vi.mock('../lib/store', () => ({
  useAppStore: vi.fn(),
}));

describe('HelpView', () => {
  it('renders all required guidance sections', () => {
    render(<HelpView />);
    expect(screen.getByText(/Secure Context Required/)).toBeDefined();
    expect(screen.getByText(/Supported Browsers/)).toBeDefined();
    expect(screen.getByText(/Serial Ownership/)).toBeDefined();
    expect(screen.getByText(/Flashing Recovery/)).toBeDefined();
    expect(screen.getByText(/Config Transport Limitations/)).toBeDefined();
    expect(screen.getByText(/Same-Site Artifacts/)).toBeDefined();
  });
});

describe('ValidateView', () => {
  beforeEach(() => {
    (useAppStore as any).mockReturnValue({
      hasGamepadApi: true,
    });
  });

  it('renders waiting state when no controller is active', () => {
    // Mock navigator.getGamepads to return empty
    global.navigator.getGamepads = vi.fn().mockReturnValue([]);

    render(<ValidateView />);
    expect(screen.getByText(/No controllers detected/)).toBeDefined();
    expect(screen.getByText(/Troubleshooting/)).toBeDefined();
  });

  it('renders error message when Gamepad API is unavailable', () => {
    (useAppStore as any).mockReturnValue({
      hasGamepadApi: false,
    });
    render(<ValidateView />);
    expect(screen.getByText(/Gamepad API Unavailable/)).toBeDefined();
  });
});

describe('FlashView Rendering', () => {
  beforeEach(() => {
    (useAppStore as any).mockReturnValue({
      serialOwner: 'none',
      hasWebSerial: true,
      isSecureContext: true,
    });
  });

  it('renders artifact selection options', () => {
    render(<FlashView />);
    expect(screen.getByText(/Same-site manifest/)).toBeDefined();
    expect(screen.getByText(/Manual local import/)).toBeDefined();
  });

  it('renders busy state when console owns the port', () => {
    (useAppStore as any).mockReturnValue({
      serialOwner: 'console',
      hasWebSerial: true,
      isSecureContext: true,
    });
    render(<FlashView />);
    expect(screen.getByText(/Serial Port Busy/)).toBeDefined();
    expect(screen.getByText(/owned by the Console/)).toBeDefined();
  });
});

describe('ConsoleView Rendering', () => {
  beforeEach(() => {
    (useAppStore as any).mockReturnValue({
      serialOwner: 'none',
      hasWebSerial: true,
      isSecureContext: true,
    });
  });

  it('renders console controls', () => {
    render(<ConsoleView />);
    expect(screen.getByText(/Serial Console/)).toBeDefined();
    expect(screen.getByText(/Connect/)).toBeDefined();
  });

  it('renders blocked state when flasher owns the port', () => {
    (useAppStore as any).mockReturnValue({
      serialOwner: 'flash',
      hasWebSerial: true,
      isSecureContext: true,
    });
    render(<ConsoleView />);
    expect(screen.getByText(/Serial Port Busy/)).toBeDefined();
    expect(screen.getByText(/owned by the Flasher/)).toBeDefined();
  });
});
