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
//
// UNITS — read before changing any number below (OI-CHARGE-05, resolved 2026-09-15).
// `I(mA) × t(s) / A(cm²)` is **mC/cm²**, because mA × s = mC. The constant this
// was compared against was named `...UCcm2` and set to 40, so the app enforced
// 40 mC/cm² while the safety MCU enforced 40 µC/cm² — 1000× apart, with the
// mislabel hiding it.
//
// OI-CHARGE-05 did not resolve that by picking one. They are ceilings on
// DIFFERENT QUANTITIES: 40 µC/cm² is a per-PHASE pulsed limit (Shannon /
// McCreery) and belongs to the charge-balanced modalities, which is where it
// now lives (see pulsedPhaseCharge.test.ts). This file covers the DC
// per-session dose, whose ceiling is 150 mC/cm² — derived from the
// conventional-tDCS envelope in docs/tdcs_database_full.csv and checked against
// Liebetanz 2009's measured lesion threshold, where the old 40 had no source in
// the tree at all. The constant is now named for its unit AND its period.
//
// The MODEL these tests pin — per electrode, never summed — is unchanged by
// any of that, which is why only the boundary numbers moved.

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
    i => i.severity === 'error' && i.parameterKey === 'chargeDensityMCcm2');

const areaErrors = (d: NPProtocolDefinition) =>
  validateProtocol(d, HW_ONLY_LIMITS).issues.filter(
    i => i.severity === 'error' && i.parameterKey === 'electrodeAreaCm2');

describe('tDCS charge-density pre-flight (OI-CHARGE-04)', () => {
  it('divides by ONE electrode area, not the sum across the montage', () => {
    // 1 mA × 1000 s = 1000 mC. On a 35 cm² pad that is 28.6 mC/cm² — well under
    // the 150 mC/cm² ceiling.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 1000,
    ))).toHaveLength(0);

    // 1 mA × 6000 s = 6000 mC → 171.4 mC/cm² on one 35 cm² pad: over.
    // The retired summed model divided by the pair's two pads (70 cm²), got
    // 85.7 mC/cm², and accepted this. That difference IS the defect.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 6000,
    ))).toHaveLength(1);
  });

  it('does not get more permissive as electrodes are added', () => {
    // The summed model divided by 35 × electrodeCount, so adding a pair made
    // the SAME current and duration pass — density per electrode is unchanged
    // by how many electrodes exist, and three pairs made it 8.4× permissive.
    const overOnePair = tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 6000);
    const overThreePairs = tdcsProtocol(
      {
        intensityMilliamps: 1, electrodeAreaCm2: 35,
        electrodePairs: [['F3', 'F4'], ['P3', 'P4'], ['Fz', 'Pz']],
      }, 6000);
    expect(chargeErrors(overOnePair)).toHaveLength(1);
    expect(chargeErrors(overThreePairs)).toHaveLength(1);
  });

  it('is stricter on a smaller declared pad, which is the point of declaring it', () => {
    // The same protocol on a 25 cm² pad (what the safety MCU used to assume
    // regardless of what the app believed) is 150 mC/cm² at 3750 s — at the
    // ceiling, which the enforcer trips on — and over it at 3800 s.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 25 }, 3749))).toHaveLength(0);
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 25 }, 3800))).toHaveLength(1);
  });

  it('rejects the ceiling exactly, because the enforcer does', () => {
    // 1.4 mA × 3750 s / 35 cm² = 150.0 mC/cm².
    //
    // This used to assert the opposite — that landing exactly ON the ceiling
    // was accepted. The safety MCU trips at >= (np_charge_monitor.c), so a
    // protocol at exactly the ceiling was one the app signed and the device
    // then cut: the OI-CHARGE-04 fidelity defect in its smallest form, found
    // while adding the per-phase check whose boundary made it visible.
    // Corrected with OI-CHARGE-05; the comparators now match on both checks.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1.4, electrodeAreaCm2: 35 }, 3750))).toHaveLength(1);
    // Just under it passes.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1.4, electrodeAreaCm2: 35 }, 3749))).toHaveLength(0);
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

  // The mislabel is what let a 1000× divergence sit unnoticed between a pre-flight
  // and its enforcer, so pin the dimension itself rather than only the outcomes.
  // If someone "fixes" the ceiling by moving the constant instead of resolving
  // OI-CHARGE-05, this fails and says why.
  it('computes mC/cm², not µC/cm² — the dimension the label got wrong', () => {
    // 1 mA through a 1 cm² electrode for 1 s delivers 1 mC ⇒ 1 mC/cm².
    // Under the retired "µC/cm²" reading the same protocol would read 1000.
    const oneMilliCoulomb = tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 1, electrodePairs: [['F3', 'F4']] }, 1);
    expect(chargeErrors(oneMilliCoulomb)).toHaveLength(0);   // 1 ≤ 150

    // 151 s of the same is 151 mC/cm² — just over. If the constant were ever
    // reinterpreted as µC/cm² without re-deriving it, this would fail at 0.151 s.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1, electrodeAreaCm2: 1, electrodePairs: [['F3', 'F4']] }, 151),
    )).toHaveLength(1);
  });

  // The clinical consequence OI-CHARGE-05 (d) named, pinned so a later change
  // to the ceiling cannot quietly take it away again. At the retired 40 the
  // routine protocol was unavailable; 13 of the 14 shipped predefined tDCS
  // protocols exceeded it.
  it('leaves the routine 2 mA × 20 min protocol available', () => {
    // 2 mA × 1200 s / 35 cm² = 68.6 mC/cm² — the single most common protocol
    // in the literature and the median of docs/tdcs_database_full.csv.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 2, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 1200,
    ))).toHaveLength(0);

    // And the longest shipped protocol: 1.5 mA × 40 min / 35 cm² = 102.9.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 1.5, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 2400,
    ))).toHaveLength(0);

    // The ceiling is still a ceiling: 2 mA × 45 min / 35 cm² = 154.3, over.
    expect(chargeErrors(tdcsProtocol(
      { intensityMilliamps: 2, electrodeAreaCm2: 35, electrodePairs: [['F3', 'F4']] }, 2700,
    ))).toHaveLength(1);
  });

  it('does not double-report: a zero area gives an area error, not a density one', () => {
    // Dividing by zero would otherwise produce an Infinity mC/cm² message on
    // top of the real diagnosis.
    expect(chargeErrors(tdcsProtocol({ electrodeAreaCm2: 0 }, 6000))).toHaveLength(0);
  });
});
