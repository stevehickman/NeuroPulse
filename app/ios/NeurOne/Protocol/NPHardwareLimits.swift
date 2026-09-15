import Foundation

// MARK: - NPHardwareLimits
// Non-overridable firmware-enforced constraints from CLAUDE.md.
// These are absolute ceilings — no dosage limit or clinician override can exceed them.
// The app enforces these as .error validation issues; the firmware also enforces them independently.

enum NPHardwareLimits {

    // MARK: PBM Transcranial / Intranasal
    // RISK-03: 400 mW/cm² pulsed claim pending regulatory opinion — not displayed in marketing copy
    static let pbmDutyCycleMaxPercent: Int = 25         // firmware-enforced unconditionally
    static let pbmPulsedPeakMWcm2: Double = 400         // pending RISK-03 regulatory opinion
    static let pbmCWMaxMWcm2: Double = 200

    // MARK: BES / tACS (T1 — consumer: Brainwave Entrainment Stimulation)
    static let besTacsMaxMilliamps: Double = 1.0
    static let besTacsMinHz: Double = 0.5
    static let besTacsMaxHz: Double = 40.0

    // MARK: tDCS (T1 — consumer: Cortical Priming Stimulation)
    static let tdcsMinMilliamps: Double = 0.1
    static let tdcsMaxMilliamps: Double = 2.0
    /// DC per-SESSION charge density ceiling, mC/cm² per electrode. Safety MCU enforced;
    /// the app cannot override it, and this pre-flight exists so the app does not sign a
    /// protocol the enforcer will cut.
    ///
    /// OI-CHARGE-05 (a)(d) 2026-09-15 — RENAMED AND RE-SOURCED. This was
    /// `tdcsMaxChargeDensityUCcm2 = 40.0`, a misnomer that concealed a 1000× divergence:
    /// `I(mA) × t(s) / A(cm²)` is mC/cm², so the clients enforced 40 mC/cm² while the
    /// safety MCU enforced 40 µC/cm². The name now carries the unit AND the period,
    /// because each ambiguity hid a different defect — the unit hid the 1000×, and the
    /// missing period hid that a per-phase PULSED figure was being applied to a DC
    /// session dose. The 40 µC/cm² figure is correct in its own right and now lives on
    /// `pulsedMaxPhaseChargeDensityUCcm2` below.
    ///
    /// 150 mC/cm² is derived, where the old 40 had no source anywhere in the document
    /// tree (the citation chain was circular — NP-DT-001 cited NP-SW-001 cited
    /// CLAUDE.md). It bounds the conventional large-pad human tDCS envelope: the
    /// most-exposed large RCT protocol in docs/tdcs_database_full.csv is Brunoni
    /// ELECT-TDCS 2013/2017 at 2 mA × 30 min on 25 cm² = 144 mC/cm². Margin check: 35×
    /// below Liebetanz 2009's measured rat epicranial DC lesion threshold of
    /// 5,240 mC/cm². Full derivation: NP-DT-001 DI-SAFE-01.
    static let tdcsMaxSessionChargeDensityMCcm2: Double = 150.0

    /// Per-PHASE charge density ceiling, µC/cm² per electrode, for the charge-balanced
    /// biphasic modalities — BES/tACS, VNS, cervical VNS, clinical tACS (OI-CHARGE-05 b).
    ///
    /// Net delivered charge on these is ~zero by construction, so a session-cumulative
    /// dose is not a physical quantity for them and is deliberately not checked. What has
    /// a damage threshold behind it is charge per phase: amplitude × phase width.
    /// 40 µC/cm² is real and correct in exactly this form (Shannon 1992 / McCreery 1990
    /// pulsed charge-injection limits) — it is the figure the tree has always carried,
    /// now applied to the waveform class it is actually about.
    static let pulsedMaxPhaseChargeDensityUCcm2: Double = 40.0

    /// Electrode areas (cm²) for the modalities whose electrodes are FIXED PARTS OF THE
    /// PRODUCT rather than a user-chosen consumable. The tDCS pad is authored per
    /// protocol precisely because the user picks it (OI-CHARGE-04); the BES pads, the
    /// auricular clip and the cervical collar are not swappable, so their area is a
    /// device property the app and firmware may both hold.
    ///
    /// ⚠ PROVISIONAL — NOT MEASURED (OI-CHARGE-07). Mirrors
    /// firmware/hub_control/include/np_hub_config.h. A SMALLER area is a SMALLER charge
    /// budget, so these may be revised down freely and only revised up against a
    /// measurement.
    static let besElectrodeAreaCm2: Double = 25.0
    static let vnsElectrodeAreaCm2: Double = 0.5
    static let cervicalVnsElectrodeAreaCm2: Double = 2.0

    /// Pulse width (seconds) the firmware substitutes when a descriptor declares none —
    /// np_mod_cvns.c and np_session_runner.c both default to 250 µs. The pre-flight must
    /// use the same number or it validates a different waveform than the one that runs.
    static let vnsDefaultPulseWidthSeconds: Double = 250e-6
    /// Default per-electrode pad area (cm²) for a NEWLY AUTHORED tDCS block — an editor
    /// default only, not an assumption any check falls back on. 35 cm² is a standard tDCS
    /// sponge pad, which is what DHF Rev 11 recorded; that record stands, and is now
    /// transmitted rather than assumed.
    ///
    /// OI-CHARGE-04 (closed 2026-09-09): the area a protocol actually validates against is
    /// `NPTDCSParams.electrodeAreaCm2`, authored per protocol and carried in the signed
    /// session descriptor (`np_mod_tdcs_params_t.electrode_area_mcm2`). The safety MCU
    /// derives its charge limit from that same declared number, so there is no longer an
    /// app-side assumption for the enforcer to disagree with. `NP_ELECTRODE_AREA_CM2 = 25`
    /// remains the MCU's fallback for channels that declare no geometry — which tDCS no
    /// longer is, because the MCU's geometry gate keeps the channel disabled rather than
    /// falling back. Neither 25 nor 35 changed; what changed is that one of them now
    /// travels to the enforcer instead of being guessed at both ends.
    static let tdcsDefaultElectrodeAreaCm2: Double = 35.0
    /// Largest declarable per-electrode area. Not a clinical limit — the wire field
    /// (`electrode_area_mcm2`) is a uint16 of milli-cm², so 65.535 cm² is what fits.
    static let tdcsMaxElectrodeAreaCm2: Double = 65.535
    static let tdcsRampSeconds: Int = 30                 // hardware-enforced, always applied
    static let tdcsMaxElectrodePairs: Int = 3

    // MARK: VNS + HRV (T1 — auricular)
    static let vnsMaxMilliamps: Double = 2.0
    static let vnsMinHz: Double = 1.0
    static let vnsMaxHz: Double = 25.0

    // MARK: Visual Stimulation (T1)
    static let visualMaxHz: Double = 100.0
    // 3–30 Hz is the photoparoxysmal risk zone — requires clinician unlock per CLAUDE.md §3
    static let visualHighRiskMinHz: Double = 3.0
    static let visualHighRiskMaxHz: Double = 30.0

    // MARK: Clinical tACS (T2)
    static let clinicalTacsMaxMilliamps: Double = 4.0
    // One driver channel per T2 cap electrode (NP-FW-HD-001 §6.4,
    // NP_HD_DRIVER_CHANNELS). OI-TACS-01: the hub wire mask carried only 16 bits
    // until 2026-09-07, so a protocol authoring more was silently clamped at
    // compile time; np_mod_clin_tacs_params_t now has a third mask byte.
    static let clinicalTacsMaxChannels: Int = 21

    // MARK: HD-tDCS (T2) — Bikson lab safety limits for 3.5mm Ag/AgCl dual-rated electrode
    static let hdTdcsMaxMilliampsPerElectrode: Double = 2.0
    static let hdTdcsMaxCurrentDensityAm2: Double = 6.0 // ≤6 A/m² per Bikson lab limits
    /// Per-phase ceiling on the HD electrode (µC/cm²); Class B `np_hd_stim.c` aborts at
    /// 95% of it. Same quantity as `pulsedMaxPhaseChargeDensityUCcm2` — kept separate
    /// because the T2 software abort is a distinct control from the Class C interlock.
    static let hdTdcsMaxChargeDensityUCcm2: Double = 40.0
    /// 3.5mm Ag/AgCl sintered electrode: π × (0.175 cm)². Mirrors NP_HD_ELECTRODE_AREA_CM2.
    /// Measured geometry, unlike the three provisional areas above.
    static let hdTdcsElectrodeAreaCm2: Double = 0.0962

    // MARK: Cervical VNS (T2) — cardiac interlock always on, non-overridable by safety MCU
    // More conservative than electroCore gammaCore predicate (≤24 mA) — ≤2 mA per CLAUDE.md
    static let cervicalVnsMaxMilliamps: Double = 2.0

    // MARK: Vibrotactile 40Hz (Accessory — provisional, HOPE Phase 3 pending)
    static let vibrotactileFrequencyHz: Double = 40.0      // firmware-locked, display only
    static let vibrotactileFreqToleranceHz: Double = 0.5   // ± 0.5 Hz
    static let vibrotactileMaxG: Double = 1.2
    static let vibrotactileMinG: Double = 0.6

    // MARK: Deep PBM 1170nm (T2)
    // 1170nm laser diodes, TEC-stabilized, 35–40mm subcortical depth
    static let deepPBMMaxMWcm2: Double = 1000.0            // ≤1,000 mW/cm² per CLAUDE.md §3

    // MARK: Aggregate PBM irradiance ceiling (OI-PBM-05 — three-channel combined)
    // Pending RISK-03 regulatory opinion scope expansion (Q4–Q6)
    static let pbmAggregateMaxMWcm2: Double = 600.0
}
