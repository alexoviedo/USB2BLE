import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { ConfigView } from '../components/views/ConfigView';
import { useAppStore } from '../lib/store';

const transportMock = {
  connect: vi.fn(),
  sendCommand: vi.fn(),
  disconnect: vi.fn(),
};

vi.mock('../lib/store', () => ({
  useAppStore: Object.assign(vi.fn(), {
    getState: vi.fn(),
  }),
}));

vi.mock('../lib/adapters/SerialConfigTransport', () => ({
  SerialConfigTransport: class MockSerialConfigTransport {
    connect = transportMock.connect;
    sendCommand = transportMock.sendCommand;
    disconnect = transportMock.disconnect;
  },
}));

describe('ConfigView', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    localStorage.clear();

    transportMock.connect.mockResolvedValue(undefined);
    transportMock.disconnect.mockResolvedValue(undefined);
    transportMock.sendCommand.mockResolvedValue({
      status: 'kOk',
      payload: {
        mapping_bundle: { bundle_id: 7, version: 1, integrity: 999 },
        profile_id: 1,
      },
    });

    const mockState = {
      serialOwner: 'none',
      hasWebSerial: true,
      isSecureContext: true,
      setSerialOwner: vi.fn(),
      setSerialPermissionState: vi.fn(),
    };
    (useAppStore as any).mockReturnValue(mockState);
    (useAppStore.getState as any).mockReturnValue(mockState);
  });

  it('renders constrained controls for both supported profiles', () => {
    render(<ConfigView />);

    expect(screen.getByLabelText(/Generic BLE Gamepad/)).toBeDefined();
    expect(screen.getByLabelText(/Wireless Xbox Controller/)).toBeDefined();
    expect(screen.getByText(/does not dynamically enumerate profile IDs/i)).toBeDefined();
  });

  it('persists the default profile 1 selection', async () => {
    render(<ConfigView />);

    fireEvent.click(screen.getByText(/Persist to Device/));

    await waitFor(() => expect(transportMock.sendCommand).toHaveBeenCalled());
    expect(transportMock.sendCommand.mock.calls[0][0].payload.profile_id).toBe(1);
    expect(screen.getByText(/Persisted Generic BLE Gamepad to device/)).toBeDefined();
  });

  it('persists profile 2 when selected and reflects it in UI state', async () => {
    transportMock.sendCommand.mockResolvedValue({
      status: 'kOk',
      payload: {
        mapping_bundle: { bundle_id: 8, version: 2, integrity: 1111 },
        profile_id: 2,
      },
    });

    render(<ConfigView />);

    fireEvent.click(screen.getByLabelText(/Wireless Xbox Controller/));
    fireEvent.click(screen.getByText(/Persist to Device/));

    await waitFor(() => expect(transportMock.sendCommand).toHaveBeenCalled());
    expect(transportMock.sendCommand.mock.calls[0][0].payload.profile_id).toBe(2);
    expect(screen.getByText(/Persisted Wireless Xbox Controller to device/)).toBeDefined();
    expect(screen.getByText(/Wireless Xbox Controller \(`profile_id=2`\)/)).toBeDefined();
  });

  it('syncs the selector from a load response', async () => {
    transportMock.sendCommand.mockResolvedValue({
      status: 'kOk',
      payload: {
        mapping_bundle: { bundle_id: 9, version: 4, integrity: 2222 },
        profile_id: 2,
      },
    });

    render(<ConfigView />);

    fireEvent.click(screen.getByText(/Load Current/));

    await waitFor(() => expect(transportMock.sendCommand).toHaveBeenCalled());
    expect((screen.getByLabelText(/Wireless Xbox Controller/) as HTMLInputElement).checked).toBe(true);
    expect(screen.getByText(/Loaded persisted device configuration metadata/)).toBeDefined();
  });

  it('surfaces unsupported profile rejections with a profile-specific message', async () => {
    transportMock.sendCommand.mockResolvedValue({
      status: 'kRejected',
      fault: {
        category: 'kUnsupportedCapability',
        reason: 5,
      },
    });

    render(<ConfigView />);

    fireEvent.click(screen.getByLabelText(/Wireless Xbox Controller/));
    fireEvent.click(screen.getByText(/Persist to Device/));

    await waitFor(() =>
      expect(screen.getByText(/Device rejected Wireless Xbox Controller \(profile_id=2\) on the config path/)).toBeDefined(),
    );
  });

  it('shows an error if load returns an unsupported profile id', async () => {
    transportMock.sendCommand.mockResolvedValue({
      status: 'kOk',
      payload: {
        mapping_bundle: { bundle_id: 10, version: 5, integrity: 3333 },
        profile_id: 99,
      },
    });

    render(<ConfigView />);

    fireEvent.click(screen.getByText(/Load Current/));

    await waitFor(() =>
      expect(screen.getByText(/config.load returned unsupported profile_id=99/)).toBeDefined(),
    );
    expect(screen.getByText(/Unsupported profile_id=99/)).toBeDefined();
  });
});
