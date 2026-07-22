package life.neurone.core.protocol

// Port of iOS NPHardwareLimits (app/ios/NeurOne/Protocol/NPHardwareLimits.swift).
// Non-overridable firmware-enforced constraints from CLAUDE.md. These are absolute
// ceilings — no dosage limit or clinician override can exceed them. The app enforces
// them as `.error` validation issues; the firmware also enforces them independently.
object NPHardwareLimits {

    // PBM Transcranial / Intranasal
    // RISK-03: 400 mW/cm² pulsed claim pending regulatory opinion — not in marketing copy.
    const val PBM_DUTY_CYCLE_MAX_PERCENT: Int = 25       // firmware-enforced unconditionally
    const val PBM_PULSED_PEAK_MW_CM2: Double = 400.0     // pending RISK-03 regulatory opinion
    const val PBM_CW_MAX_MW_CM2: Double = 200.0

    // BES / tACS (T1 — consumer: Brainwave Entrainment Stimulation)
    const val BES_TACS_MAX_MILLIAMPS: Double = 1.0
    const val BES_TACS_MIN_HZ: Double = 0.5
    const val BES_TACS_MAX_HZ: Double = 40.0

    // tDCS (T1 — consumer: Cortical Priming Stimulation)
    const val TDCS_MIN_MILLIAMPS: Double = 0.1
    const val TDCS_MAX_MILLIAMPS: Double = 2.0
    const val TDCS_MAX_CHARGE_DENSITY_UC_CM2: Double = 40.0 // safety MCU enforced, app cannot override
    /** Assumed area per electrode (cm²) for charge-density estimation when geometry is absent. */
    const val TDCS_DEFAULT_ELECTRODE_AREA_CM2: Double = 35.0
    const val TDCS_RAMP_SECONDS: Int = 30                // hardware-enforced, always applied
    const val TDCS_MAX_ELECTRODE_PAIRS: Int = 3

    // VNS + HRV (T1 — auricular)
    const val VNS_MAX_MILLIAMPS: Double = 2.0
    const val VNS_MIN_HZ: Double = 1.0
    const val VNS_MAX_HZ: Double = 25.0

    // Visual Stimulation (T1)
    const val VISUAL_MAX_HZ: Double = 100.0
    // 3–30 Hz is the photoparoxysmal risk zone — requires clinician unlock per CLAUDE.md §3.
    const val VISUAL_HIGH_RISK_MIN_HZ: Double = 3.0
    const val VISUAL_HIGH_RISK_MAX_HZ: Double = 30.0

    // Clinical tACS (T2)
    const val CLINICAL_TACS_MAX_MILLIAMPS: Double = 4.0

    // HD-tDCS (T2) — Bikson lab safety limits for 3.5mm Ag/AgCl dual-rated electrode
    const val HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE: Double = 2.0
    const val HD_TDCS_MAX_CURRENT_DENSITY_A_M2: Double = 6.0   // ≤6 A/m² per Bikson lab limits
    const val HD_TDCS_MAX_CHARGE_DENSITY_UC_CM2: Double = 40.0 // 95% of 40µC/cm² trips software abort

    // Cervical VNS (T2) — cardiac interlock always on; ≤2 mA (vs electroCore ≤24 mA predicate)
    const val CERVICAL_VNS_MAX_MILLIAMPS: Double = 2.0

    // Vibrotactile 40Hz (Accessory — provisional, HOPE Phase 3 pending)
    const val VIBROTACTILE_FREQUENCY_HZ: Double = 40.0
    const val VIBROTACTILE_FREQ_TOLERANCE_HZ: Double = 0.5
    const val VIBROTACTILE_MAX_G: Double = 1.2
    const val VIBROTACTILE_MIN_G: Double = 0.6

    // Deep PBM 1170nm (T2) — ≤1,000 mW/cm² per CLAUDE.md §3
    const val DEEP_PBM_MAX_MW_CM2: Double = 1000.0

    // Aggregate PBM irradiance ceiling (OI-PBM-05 — three-channel combined; pending RISK-03 Q4–Q6)
    const val PBM_AGGREGATE_MAX_MW_CM2: Double = 600.0
}
