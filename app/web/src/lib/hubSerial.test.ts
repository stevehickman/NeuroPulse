import { describe, expect, it } from 'vitest';
import { compileProtocol } from './hubCompiler';
import { DEVICE_SERIAL_LEN, DEVICE_SERIAL_UUID, parseDeviceSerial } from './hubSerial';

describe('hubSerial (OI-AND-WIRE-02)', () => {
  it('accepts exactly 32 bytes, as a copy', () => {
    const v = Uint8Array.from({ length: DEVICE_SERIAL_LEN }, (_, i) => 0x40 + i);
    const s = parseDeviceSerial(v)!;
    expect(Array.from(s)).toEqual(Array.from(v));
    v[0] = 0;
    expect(s[0]).toBe(0x40);
  });

  it('refuses short, long, empty and absent values', () => {
    expect(parseDeviceSerial(new Uint8Array(31))).toBeNull();
    expect(parseDeviceSerial(new Uint8Array(33))).toBeNull();
    expect(parseDeviceSerial(new Uint8Array(0))).toBeNull();
    expect(parseDeviceSerial(null)).toBeNull();
    expect(parseDeviceSerial(undefined)).toBeNull();
  });

  it('uses the UUID the hub, iOS and Android carry', () => {
    expect(DEVICE_SERIAL_UUID).toBe('4e455550-0017-1000-8000-00805f9b34fb');
  });

  it('the parsed serial lands at header offset 28', () => {
    const serial = parseDeviceSerial(Uint8Array.from({ length: 32 }, (_, i) => i + 1))!;
    const { blob } = compileProtocol(
      {
        name: 'S',
        timingMode: { type: 'duration', seconds: 60 },
        modalities: [{
          id: 'm1',
          enabled: true,
          interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
          modalityParams: {
            type: 'eeg_neurofeedback',
            params: { channels: 'all', band: 'alpha', closedLoopEnabled: true },
          },
        }],
      } as never,
      { deviceSerial: serial },
    );
    expect(Array.from(blob.slice(28, 60))).toEqual(Array.from(serial));
  });
});
