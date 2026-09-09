import { describe, it, expect } from 'vitest';
import { validateProtocol } from './protocolValidator';
import { defaultParams } from '../types/protocol';
import type { NPProtocolDefinition, TDCSParams } from '../types/protocol';
import type { NPLimitsSet } from '../types/limits';

// OI-CHARGE-04 (closed 2026-09-09).
//
// The defect these tests pin was NOT that the app used the wrong number. It was
// that the app and the safety MCU used DIFFERENT numbers, and nothing in the
// codebase tied them together. iOS and Android divided by 35 cm² × the count of
// every electrode in the montage; the MCU divided by NP_ELECTRODE_AREA_CM2 = 25
// per electrode. For one pair that is 70 cm² against 25 cm² — a pre-flight
// 2.8× more permissive than the enforcer, so the app accepted protocols the MCU
// terminated partway through, presenting as an unexplained mid-session stop.
//
// The resolution is not a new constant. The area is authored per protocol,
// travels in the signed descriptor, and both sides divide by that one declared
// number. What these tests guard is the MODEL: per electrode, never summed.

const HW_ONLY_LIMITS: NPLimitsSet = {
  id: 'test-hw-only',
  name: 'Hardware only',
  description: 'No dosage limits — hardware ceilings alone.',
  createdAt: '2026-09-09T00:00:00Z',
  modifiedAt: '2026-09-09T00:00:00Z',
  level: 'global',
};

function tdcsProtocol(params: Partial<TDCSParams>, durationSeconds: number): NPProtocolDefinition {
  return {
    id: '20000001-0000-0000-0000-000000000000',
    name: 'tDCS charge density fixture',
    description: '',
    author: 'test',
    version: '1.0',
    tags: [],
    createdAt: '2026-09-09T00:00:00Z',
    modifiedAt: '2026-09-09T00:00:00Z',
    isPredefined: false,
    timingMode: { type: 'duration', seconds: durationSeconds },
    modalities: [{
      id: 'm1',
      modalityParams: { type: 'tdcs', params: { ...defaultParams('tdcs'), ...params } },
      interval: { intervalOnSeconds: durationSeconds, intervalOffSeconds: 0 },
      enabled: true,
    }],
  };
}

const chargeErrors = (d: NPProtocolDefinition) =>
  validateProtocol(d, HW_ONLY_LIMITS).issues.filter(
    i => i.severity === 'error' && i.parameterKey === 'chargeDensityUCcm2');

const areaErrors = (d: NPProtocolDefinition) =>
  validateProtocol(d, HW_ONLY_LIMITS).issues.filter(
    i => i.severity === 'error' && i.parameterKey === 'electrodeAreaCm2');

describe('tDCS charge-density pre-flight (OI-CHARGE-04)', () => {
  it('divides by ONE electrode area, not the sum across the montage', () => {
    // 1 mA × 1000 s = 1000 µC. On a 35 cm² pad that is 28.6 µC/cm² — under the
    // 40 µC/cm² ceiling. Summing the pair's two pads (70 cm²) would give
    // 14.3 µC/cm², which is also under; the pairs below are what separate the
    // two models.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 1000,
    ))).toHaveLength(0);

    // 1 mA × 1500 s = 1500 µC → 42.9 µC/cm² on one 35 cm² pad: over.
    // The retired summed model gave 21.4 µC/cm² and accepted this.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 1500,
    ))).toHaveLength(1);
  });

  it('does not get more permissive as electrodes are added', () => {
    // The summed model divided by 35 × electrodeCount, so adding a pair made
    // the SAME current and duration pass — density per electrode is unchanged
    // by how many electrodes exist, and three pairs made it 8.4× permissive.
    const overOnePair = tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 1500);
    const overThreePairs = tdcsProtocol(
      {
        intensityMilliamps: 1, electrodeAreaCm2: 35,
        electrodePairs: [['F3', 'F4'], ['P3', 'P4'], ['Fz', 'Pz']],
      }, 1500);
    expect(chargeErrors(overOnePair)).toHaveLength(1);
    expect(chargeErrors(overThreePairs)).toHaveLength(1);
  });

  it('is stricter on a smaller declared pad, which is the point of declaring it', () => {
    // The same protocol on a 25 cm² pad (what the safety MCU used to assume
    // regardless of what the app believed) is 40 µC/cm² at 1000 s — exactly at
    // the inclusive ceiling — and over it at 1100 s.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 25 }, 1000))).toHaveLength(0);
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 25 }, 1100))).toHaveLength(1);
  });

  it('accepts the ceiling exactly (inclusive) and rejects just past it', () => {
    // 1.4 mA × 1000 s / 35 cm² = 40.0 µC/cm².
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1.4, electrodeAreaCm2: 35 }, 1000))).toHaveLength(0);
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1.4, electrodeAreaCm2: 35 }, 1001))).toHaveLength(1);
  });

  it('rejects an undeclared or unencodable area rather than assuming one', () => {
    // 0 is what an un-migrated protocol compiles to. The hub refuses it and the
    // safety MCU's geometry gate holds tDCS off, so accepting it here would
    // hand the user a session that silently never stimulates.
    expect(areaErrors(tdcsProtocol({ electrodeAreaCm2: 0 }, 600))).toHaveLength(1);
    expect(areaErrors(tdcsProtocol({ electrodeAreaCm2: -5 }, 600))).toHaveLength(1);
    // Above what the uint16 milli-cm² wire field can carry.
    expect(areaErrors(tdcsProtocol({ electrodeAreaCm2: 70 }, 600))).toHaveLength(1);
    expect(areaErrors(tdcsProtocol({ electrodeAreaCm2: 35 }, 600))).toHaveLength(0);
  });

  it('does not double-report: a zero area gives an area error, not a density one', () => {
    // Dividing by zero would otherwise produce an Infinity µC/cm² message on
    // top of the real diagnosis.
    expect(chargeErrors(tdcsProtocol({ electrodeAreaCm2: 0 }, 6000))).toHaveLength(0);
  });
});
