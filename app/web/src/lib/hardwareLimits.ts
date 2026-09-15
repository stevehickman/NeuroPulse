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
  //
  // OI-CHARGE-05 (a)(d) 2026-09-15: RENAMED AND RE-SOURCED. This used to be
  // `tdcsMaxChargeDensityUCcm2: 40.0`, a misnomer that concealed a 1000×
  // divergence — `I(mA) × t(s) / A(cm²)` is mC/cm², so the clients enforced
  // 40 mC/cm² while the safety MCU enforced 40 µC/cm². The name now states
  // the unit AND the period, because both were ambiguous and each hid a
  // different defect: the unit hid the 1000×, and the missing period hid
  // that a per-phase pulsed figure was being applied to a DC session dose.
  //
  // 150 mC/cm² per session per electrode, derived rather than asserted
  // (the old 40 had no source anywhere in the tree — the citation chain was
  // circular). Bounds the conventional large-pad human tDCS envelope: the
  // most-exposed large RCT protocol in docs/tdcs_database_full.csv is Brunoni
  // ELECT-TDCS 2013/2017 at 2 mA × 30 min on 25 cm² = 144 mC/cm². Margin
  // check: 35× below Liebetanz 2009's measured rat epicranial DC lesion
  // threshold of 5,240 mC/cm². Full derivation: NP-DT-001 DI-SAFE-01.
  tdcsMaxSessionChargeDensityMCcm2: 150.0,

  // Per-PHASE ceiling for the charge-balanced biphasic modalities — BES/tACS,
  // VNS, cervical VNS, clinical tACS (OI-CHARGE-05 (b)). 40 µC/cm² is a real
  // and correct figure in this form (Shannon 1992 / McCreery 1990 pulsed
  // charge-injection limits); it is the number the tree has always carried,
  // now applied to the waveform class it is actually about. Net charge on
  // these modalities is ~zero by construction, so a session-cumulative dose
  // is not a physical quantity for them and is not checked.
  pulsedMaxPhaseChargeDensityUCcm2: 40.0,

  // Electrode areas for the modalities whose electrodes are FIXED PARTS OF
  // THE PRODUCT rather than a user-chosen consumable. The tDCS pad is
  // authored per protocol precisely because the user picks it (OI-CHARGE-04);
  // the BES pads, auricular clip and cervical collar are not swappable, so
  // their area is a device property both the app and firmware may hold.
  // ⚠ PROVISIONAL — NOT MEASURED (OI-CHARGE-07). Mirrors
  // firmware/hub_control/include/np_hub_config.h. A SMALLER area is a
  // SMALLER charge budget, so these may be revised down freely and may only
  // be revised up against a measurement.
  besElectrodeAreaCm2: 25.0,
  vnsElectrodeAreaCm2: 0.5,
  cervicalVnsElectrodeAreaCm2: 2.0,
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
  // HD-tDCS 3.5mm Ag/AgCl sintered electrode: π × (0.175 cm)². Mirrors
  // NP_HD_ELECTRODE_AREA_CM2. Measured geometry, unlike the three above.
  hdTdcsElectrodeAreaCm2: 0.0962,

  // Cervical VNS (T2 accessory, cardiac interlock always active)
  cervicalVnsMaxMilliamps: 2.0,

  // 40Hz Vibrotactile (provisional accessory)
  vibrotactileFrequencyHz: 40.0,
  vibrotactileFreqToleranceHz: 0.5,
  vibrotactileMaxG: 1.2,
  vibrotactileMinG: 0.6,
} as const;
