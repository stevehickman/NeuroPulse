// ─── Hardware limits ───────────────────────────────────────────────────────────
// These are the absolute physical constraints of the NeuroPulse hardware.
// They cannot be overridden by any software configuration.

export const NPHardwareLimits = {
  // PBM Transcranial / Intranasal (shared firmware limit)
  pbmDutyCycleMaxPercent: 25,
  pbmPulsedPeakMWcm2: 400,   // pending RISK-03 regulatory opinion
  pbmCWMaxMWcm2: 200,

  // BES / tACS (consumer: Brainwave Entrainment Stimulation)
  besTacsMaxMilliamps: 1.0,
  besTacsMinHz: 0.5,
  besTacsMaxHz: 40.0,

  // tDCS (consumer: Cortical Priming Stimulation)
  tdcsMinMilliamps: 0.1,
  tdcsMaxMilliamps: 2.0,
  tdcsMaxChargeDensityUCcm2: 40.0,  // safety MCU enforced, app cannot override
  tdcsRampSeconds: 30,               // hardware-enforced minimum ramp
  tdcsMaxElectrodePairs: 3,

  // VNS (auricular)
  vnsMaxMilliamps: 2.0,
  vnsMinHz: 1.0,
  vnsMaxHz: 25.0,

  // Visual stimulation
  visualMaxHz: 100.0,
  visualHighRiskMinHz: 3.0,   // photoparoxysmal risk zone — clinician unlock required
  visualHighRiskMaxHz: 30.0,  // firmware halts goggles, <200ms, if EEG shows photoparoxysmal response

  // Clinical tACS (T2 only)
  clinicalTacsMaxMilliamps: 4.0,

  // HD-tDCS (T2 only, sLORETA-guided 4×1 ring montage)
  hdTdcsMaxMilliampsPerElectrode: 2.0,
  hdTdcsMaxChargeDensityAm2: 6.0,  // Bikson lab 3.5mm electrode safety limit

  // Cervical VNS (T2 accessory, cardiac interlock always active)
  cervicalVnsMaxMilliamps: 2.0,

  // 40Hz Vibrotactile (provisional accessory)
  vibrotactileFrequencyHz: 40.0,
  vibrotactileFreqToleranceHz: 0.5,
  vibrotactileMaxG: 1.2,
  vibrotactileMinG: 0.6,
} as const;
