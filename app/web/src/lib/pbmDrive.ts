// ─── Absolute dose → drive register ────────────────────────────────────────────
//
// A protocol states an absolute quantity: irradiance in mW/cm² at the scalp, or
// sound level in dB SPL at the ear (NP-NPPS-REF-001 §4.1b, §4.7). A percentage
// of "full output" does not state the stimulus, because full output is a fact
// about the emitter, driver and tile, and it moves when any of them does.
//
// This file is the ONE place those facts live. Everything above it (the
// protocol, the validator, the editor) is hardware-independent. When the
// hardware changes, this table changes and no protocol file does.
//
// A request the hardware cannot reach is REFUSED. It is never clamped to the
// nearest reachable value: a clamped pulse train is a different stimulus from
// the one authored (CLAUDE.md §3).

import type { NPPBMChannelElement } from '../../../../common/lib/wavelengthRules';
import { NPHardwareLimits } from '../../../../common/lib/constants.generated';

/**
 * Peak irradiance at the scalp that each emitter channel delivers at full drive
 * (the 255 register value), mW/cm². The figures are common/npps/constants.json's
 * (`NPHardwareLimits.PBM_FULL_SCALE_*`, `INTRANASAL_FULL_SCALE_MW_CM2`,
 * `AUDIO_MAX_DB_SPL`), the same ones the Rust compiler reads at build time, so
 * the two cannot drift (OI-NPPS-ABS-02). Their derivation and status are on
 * the constants' notes there (register rows UC-064, UC-065).
 */
export const PBM_FULL_SCALE_MW_CM2: Readonly<Record<NPPBMChannelElement, number>> = {
  led_660: NPHardwareLimits.PBM_FULL_SCALE_660_MW_CM2,
  led_808: NPHardwareLimits.PBM_FULL_SCALE_808_MW_CM2,
  led_1064: NPHardwareLimits.PBM_FULL_SCALE_1064_MW_CM2,
};

/** Irradiance the intranasal probe delivers at full drive, mW/cm². A provisional placeholder (UC-065). */
export const INTRANASAL_FULL_SCALE_MW_CM2 = NPHardwareLimits.INTRANASAL_FULL_SCALE_MW_CM2;

const EPS = 1e-9;

/** Irradiance → 0–255 current register, or throw when the channel cannot reach it. */
export function irradianceToRegister(
  irradianceMWcm2: number, fullScaleMWcm2: number, channel: string,
): number {
  if (!Number.isFinite(irradianceMWcm2) || irradianceMWcm2 <= 0) {
    throw new Error(`PBM irradiance must be positive, got ${irradianceMWcm2} mW/cm².`);
  }
  if (irradianceMWcm2 > fullScaleMWcm2 * (1 + EPS)) {
    throw new Error(
      `PBM irradiance ${irradianceMWcm2} mW/cm² exceeds what ${channel} delivers at full ` +
      `drive (${fullScaleMWcm2} mW/cm²). The protocol is refused, not reduced to fit.`,
    );
  }
  return Math.max(1, Math.min(255, Math.round(irradianceMWcm2 / fullScaleMWcm2 * 255)));
}

/** Acoustic output ceiling, dB SPL at the ear. Absolute (NP-HW-AUDIO-001 REQ-AUDIO-19). */
export const AUDIO_MAX_DB_SPL = NPHardwareLimits.AUDIO_MAX_DB_SPL;

/** dB SPL at the ear → the wire byte (2 × dB), or throw above the ceiling. Never reduces a level to fit. */
export function dbToLevelHalfDb(db: number): number {
  if (!Number.isFinite(db) || db <= 0 || db > AUDIO_MAX_DB_SPL + EPS) {
    throw new Error(
      `Audio level ${db} dB SPL is outside 0–${AUDIO_MAX_DB_SPL} dB SPL, the acoustic output ceiling. ` +
      `The protocol is refused, not reduced to fit.`,
    );
  }
  return Math.max(1, Math.min(AUDIO_MAX_DB_SPL * 2, Math.round(db * 2)));
}
