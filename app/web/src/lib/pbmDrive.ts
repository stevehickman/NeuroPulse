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

/**
 * Peak irradiance at the scalp that each emitter channel delivers at full drive
 * (the 255 register value), mW/cm².
 *
 * led_660 / led_808: NP-HW-HEXTILE-001 §4.3.1, T1-A: 45 emitters × 95 mW over
 *   10.61 cm² = 403 mW/cm² at 150 mA. A DESIGN TARGET: OI-HEXTILE-02 has
 *   selected no emitter (register row UC-064).
 * led_1064: §4.3.2, T1-C CH_C = 28 mW/cm². The emitter-efficiency wall of
 *   OI-HEXTILE-21: no protocol that asks for more can run on this channel.
 */
export const PBM_FULL_SCALE_MW_CM2: Readonly<Record<NPPBMChannelElement, number>> = {
  led_660: 403,
  led_808: 403,
  led_1064: 28,
};

/**
 * Irradiance the intranasal probe delivers at full drive, mW/cm².
 * PROVISIONAL PLACEHOLDER (register row UC-065): the probe has no owning
 * specification (OI-ART-04), so no document states what it can emit. 100 is the
 * value that keeps every shipped intranasal protocol's drive register where it
 * was when it was written as a percentage. It is a stand-in for a measurement,
 * not a capability claim.
 */
export const INTRANASAL_FULL_SCALE_MW_CM2 = 100;

/**
 * Audio calibration: dB SPL at the ear is `AUDIO_DB_AT_ZERO + AUDIO_DB_PER_PERCENT × volume_pct`,
 * volume_pct being the uncalibrated 0–100 wire field. PROVISIONAL PLACEHOLDER
 * (register row UC-066, OI-AUDIOHW-01): nothing relates that field to sound
 * pressure, and no dB SPL ceiling exists. The line keeps every shipped
 * protocol's wire value where it was and tops out at 90 dB SPL at 100.
 */
export const AUDIO_DB_AT_ZERO = 40;
export const AUDIO_DB_PER_PERCENT = 0.5;

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

/** dB SPL → 0–100 volume register, or throw when outside the calibrated range. */
export function dbToVolumePercent(db: number): number {
  const pct = (db - AUDIO_DB_AT_ZERO) / AUDIO_DB_PER_PERCENT;
  if (!Number.isFinite(db) || pct < -EPS || pct > 100 + EPS) {
    throw new Error(
      `Audio level ${db} dB SPL is outside the range the drive can deliver ` +
      `(${AUDIO_DB_AT_ZERO}–${AUDIO_DB_AT_ZERO + 100 * AUDIO_DB_PER_PERCENT} dB SPL). ` +
      `The protocol is refused, not reduced to fit.`,
    );
  }
  return Math.max(0, Math.min(100, Math.round(pct)));
}
