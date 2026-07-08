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
    static let tdcsMaxChargeDensityUCcm2: Double = 40.0 // safety MCU enforced, app cannot override
    /// Assumed area per individual electrode (cm²) used for charge-density estimation when the
    /// electrode geometry is not explicitly provided. 35 cm² is a standard tDCS sponge pad.
    /// Total area = tdcsDefaultElectrodeAreaCm2 × number of electrode positions across all pairs.
    static let tdcsDefaultElectrodeAreaCm2: Double = 35.0
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

    // MARK: HD-tDCS (T2) — Bikson lab safety limits for 3.5mm Ag/AgCl dual-rated electrode
    static let hdTdcsMaxMilliampsPerElectrode: Double = 2.0
    static let hdTdcsMaxCurrentDensityAm2: Double = 6.0 // ≤6 A/m² per Bikson lab limits
    static let hdTdcsMaxChargeDensityUCcm2: Double = 40.0 // 95% of 40µC/cm² trips software abort

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
