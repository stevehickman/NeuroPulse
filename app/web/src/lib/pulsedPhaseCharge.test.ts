import { describe, it, expect } from 'vitest';
import { validateProtocol } from './protocolValidator';
import { defaultParams } from '../types/protocol';
import type { NPProtocolDefinition, NPModalityTypeId } from '../types/protocol';
import type { NPLimitsSet } from '../types/limits';

// OI-CHARGE-05 (b) — per-PHASE charge density for the charge-balanced modalities.
//
// BES/tACS, VNS, cervical VNS and clinical tACS are charge-balanced biphasic:
// net delivered charge over a session is ~zero by construction, so the
// session-cumulative dose the DC check enforces is not a physical quantity for
// them. Applying it anyway — which is what the safety MCU did until 2026-09-15
// — would have tripped every one of them in 0.4–1.0 s at its rated current,
// against a budget meant for a DC session.
//
// What has a damage threshold behind it here is charge PER PHASE: amplitude ×
// phase width, against 40 µC/cm² (Shannon 1992 / McCreery 1990). That is the
// figure the tree always carried; OI-CHARGE-05 moved it to the waveform class
// it is actually about rather than changing it.
//
// These tests exist for pre-flight FIDELITY, the same reason OI-CHARGE-04
// existed: the app must reject what the enforcer will reject. A protocol the
// app signs and the safety MCU then refuses to grant presents to the user as a
// modality that silently never starts.

const HW_ONLY_LIMITS: NPLimitsSet = {
  id: 'test-hw-only',
  name: 'Hardware only',
  description: 'No dosage limits — hardware ceilings alone.',
  createdAt: '2026-09-15T00:00:00Z',
  modifiedAt: '2026-09-15T00:00:00Z',
  level: 'global',
};

function protocolWith(
  type: NPModalityTypeId,
  params: Record<string, unknown>,
  durationSeconds = 600,
): NPProtocolDefinition {
  return {
    id: '20000002-0000-0000-0000-000000000000',
    name: 'phase charge fixture',
    description: '',
    author: 'test',
    version: '1.0',
    tags: [],
    createdAt: '2026-09-15T00:00:00Z',
    modifiedAt: '2026-09-15T00:00:00Z',
    isPredefined: false,
    timingMode: { type: 'duration', seconds: durationSeconds },
    modalities: [{
      id: 'm1',
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      modalityParams: { type, params: { ...(defaultParams(type) as any), ...params } } as any,
      interval: { intervalOnSeconds: durationSeconds, intervalOffSeconds: 0 },
      enabled: true,
    }],
  };
}

const phaseErrors = (d: NPProtocolDefinition) =>
  validateProtocol(d, HW_ONLY_LIMITS).issues.filter(
    i => i.severity === 'error' && i.parameterKey === 'phaseChargeDensityUCcm2');

const sessionErrors = (d: NPProtocolDefinition) =>
  validateProtocol(d, HW_ONLY_LIMITS).issues.filter(
    i => i.severity === 'error' && i.parameterKey === 'chargeDensityMCcm2');

describe('pulsed / AC per-phase charge density (OI-CHARGE-05 b)', () => {
  it('never applies the DC per-session dose to a charge-balanced modality', () => {
    // An hour of BES/tACS at its 1 mA cap. Integrated as |I| × t this is
    // 3600 mC — 144 mC/cm² on a 25 cm² pad, right at the DC ceiling — but it
    // is not a dose, because the waveform is charge balanced. No session
    // error may be raised for it at any duration.
    const long = protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 10, waveform: 'sinusoidal' }, 3600);
    expect(sessionErrors(long)).toHaveLength(0);

    const veryLong = protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 10, waveform: 'sinusoidal' }, 36000);
    expect(sessionErrors(veryLong)).toHaveLength(0);
  });

  it('passes VNS at its rated worst case', () => {
    // 2 mA × 250 µs (the firmware's default pulse width) = 0.5 µC. On the
    // 0.5 cm² auricular clip pad that is 1.0 µC/cm² — 2.5% of the ceiling.
    expect(phaseErrors(protocolWith('vns_hrv',
      { intensityMilliamps: 2, frequencyHz: 25 }))).toHaveLength(0);
  });

  it('passes cervical VNS at its rated worst case', () => {
    // 2 mA × 250 µs = 0.5 µC on a 2 cm² collar pad = 0.25 µC/cm².
    expect(phaseErrors(protocolWith('cervical_vns',
      { intensityMilliamps: 2, frequencyHz: 25 }))).toHaveLength(0);
  });

  it('passes tACS across its whole authorised band', () => {
    // The band bottom is where phase charge is LARGEST — a 0.5 Hz half-period
    // is a full second. 1 mA sinusoidal on a 25 cm² pad:
    //   rectangular equivalent 1000 µC, × 2/π = 636.6 µC, ÷ 25 = 25.5 µC/cm².
    // 64% of the ceiling, so it passes — but only because the 2/π factor is
    // applied. Without it the same protocol reads 40.0 and fails.
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 0.5, waveform: 'sinusoidal' }))).toHaveLength(0);

    // And the band top, where phase charge is negligible.
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 40, waveform: 'sinusoidal' }))).toHaveLength(0);
  });

  it('applies the 2/pi factor only to sinusoids', () => {
    // Identical amplitude and phase duration; only the waveform differs.
    // A square wave delivers the full I × T: 1 mA × 1 s = 1000 µC ÷ 25 cm²
    // = 40.0 µC/cm², exactly the ceiling, so it is refused.
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 0.5, waveform: 'square' }))).toHaveLength(1);

    // The sinusoid at the same settings passes. This pair is the whole reason
    // the factor is in the model rather than rounded away — and the safety MCU
    // applies the same one, as the integer 2000/3141.
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 0.5, waveform: 'sinusoidal' }))).toHaveLength(0);
  });

  it('gets stricter as frequency falls, because the phase gets longer', () => {
    // A square wave at 1 mA: the ceiling is 40 µC/cm² × 25 cm² = 1000 µC, so
    // the longest admissible phase is 1 s — i.e. 0.5 Hz is the boundary and
    // anything slower fails. 0.4 Hz gives a 1.25 s phase = 1250 µC = 50 µC/cm².
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 0.4, waveform: 'square' }))).toHaveLength(1);

    // 1 Hz halves the phase and passes comfortably.
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 1, waveform: 'square' }))).toHaveLength(0);
  });

  it('is not raised for a modality that drives no electrode', () => {
    expect(phaseErrors(protocolWith('pbm_transcranial', {}))).toHaveLength(0);
    expect(phaseErrors(protocolWith('audio_entrainment', {}))).toHaveLength(0);
  });

  it('does not divide by zero on a zero frequency or amplitude', () => {
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 1, frequencyHz: 0, waveform: 'sinusoidal' }))).toHaveLength(0);
    expect(phaseErrors(protocolWith('bes_tacs',
      { intensityMilliamps: 0, frequencyHz: 10, waveform: 'sinusoidal' }))).toHaveLength(0);
  });
});
