// ─── Hardware limits ───────────────────────────────────────────────────────────
// These are the absolute physical constraints of the NeurOne hardware.
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
  // OI-CHARGE-04 (closed 2026-09-09): this IS checked here now. There is no
  // web electrode-area constant to add, because there is no longer a global
  // electrode area on any client — the area is authored per protocol
  // (TDCSParams.electrodeAreaCm2), travels in the signed session descriptor,
  // and the safety MCU derives its charge limit from that same declared
  // number. What used to be a 35-vs-25 disagreement between the app's
  // assumption and the enforcer's is now one transmitted value.
  // NAME IS A MISNOMER, kept for now because all three clients share it and
  // renaming it is OI-CHARGE-05's job, not OI-CHARGE-04's: the value is
  // compared against `I(mA) × t(s) / A(cm²)`, which is mC/cm². So this is
  // 40 **mC**/cm². The safety MCU separately enforces 40 µC/cm² — 1000×
  // stricter — and reconciling the two is OI-CHARGE-05.
  tdcsMaxChargeDensityUCcm2: 40.0,
  // Largest declarable per-electrode area: the wire field
  // (np_mod_tdcs_params_t.electrode_area_mcm2) is a uint16 of milli-cm², so
  // 65535 mcm². Not a clinical limit — an encoding ceiling, well above any
  // real pad (a large sponge is 35 cm²).
  tdcsMaxElectrodeAreaCm2: 65.535,
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
  // One driver channel per T2 cap electrode (NP-FW-HD-001 §6.4,
  // NP_HD_DRIVER_CHANNELS). OI-TACS-01: the hub wire mask carried only 16 bits
  // until 2026-09-07, so every encoder clamped here; np_mod_clin_tacs_params_t
  // now has a third mask byte and 21 is deliverable, not just authorable.
  clinicalTacsMaxChannels: 21,

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
