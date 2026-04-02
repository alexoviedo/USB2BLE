import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import { CapabilityBanner } from '../components/CapabilityBanner';
import { useAppStore } from '../lib/store';
import React from 'react';

// Mock the store
vi.mock('../lib/store', () => ({
  useAppStore: vi.fn(),
}));

describe('CapabilityBanner', () => {
  const setCapabilities = vi.fn();

  beforeEach(() => {
    vi.clearAllMocks();
    (useAppStore as any).mockReturnValue({
      isSecureContext: true,
      hasWebSerial: true,
      hasGamepadApi: true,
      setCapabilities,
    });
  });

  it('renders supported message when all APIs are available', () => {
    render(<CapabilityBanner />);
    expect(screen.getByText(/Environment fully supported/)).toBeDefined();
  });

  it('renders insecure context warning', () => {
    (useAppStore as any).mockReturnValue({
      isSecureContext: false,
      hasWebSerial: true,
      hasGamepadApi: true,
      setCapabilities,
    });
    render(<CapabilityBanner />);
    expect(screen.getByText(/serve over HTTPS or localhost before using Web Serial features/)).toBeDefined();
  });

  it('renders unsupported browser warning', () => {
    (useAppStore as any).mockReturnValue({
      isSecureContext: true,
      hasWebSerial: false,
      hasGamepadApi: true,
      setCapabilities,
    });
    render(<CapabilityBanner />);
    expect(screen.getByText(/use a Chromium-based desktop browser/)).toBeDefined();
  });

  it('renders gamepad unavailable warning', () => {
    (useAppStore as any).mockReturnValue({
      isSecureContext: true,
      hasWebSerial: true,
      hasGamepadApi: false,
      setCapabilities,
    });
    render(<CapabilityBanner />);
    expect(screen.getByText(/Gamepad unavailable: Validate view partially disabled/)).toBeDefined();
  });
});
