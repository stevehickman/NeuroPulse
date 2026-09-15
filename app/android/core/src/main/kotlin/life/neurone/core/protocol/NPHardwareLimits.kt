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
    /**
     * DC per-SESSION charge density ceiling, mC/cm² per electrode. Safety MCU enforced;
     * the app cannot override it, and this pre-flight exists so the app does not sign a
     * protocol the enforcer will cut.
     *
     * OI-CHARGE-05 (a)(d) 2026-09-15 — RENAMED AND RE-SOURCED. This was
     * `TDCS_MAX_CHARGE_DENSITY_UC_CM2 = 40.0`, a misnomer that concealed a 1000×
     * divergence: `I(mA) × t(s) / A(cm²)` is mC/cm², so the clients enforced 40 mC/cm²
     * while the safety MCU enforced 40 µC/cm². The name now carries the unit AND the
     * period, because each ambiguity hid a different defect — the unit hid the 1000×, and
     * the missing period hid that a per-phase PULSED figure was being applied to a DC
     * session dose. The 40 µC/cm² figure is correct in its own right and now lives on
     * [PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2] below.
     *
     * 150 mC/cm² is derived, where the old 40 had no source anywhere in the document tree
     * (the citation chain was circular). It bounds the conventional large-pad human tDCS
     * envelope: the most-exposed large RCT protocol in docs/tdcs_database_full.csv is
     * Brunoni ELECT-TDCS 2013/2017 at 2 mA × 30 min on 25 cm² = 144 mC/cm². Margin check:
     * 35× below Liebetanz 2009's measured rat epicranial DC lesion threshold of
     * 5,240 mC/cm². Full derivation: NP-DT-001 DI-SAFE-01.
     */
    const val TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2: Double = 150.0

    /**
     * Per-PHASE charge density ceiling, µC/cm² per electrode, for the charge-balanced
     * biphasic modalities — BES/tACS, VNS, cervical VNS, clinical tACS (OI-CHARGE-05 b).
     *
     * Net delivered charge on these is ~zero by construction, so a session-cumulative
     * dose is not a physical quantity for them and is deliberately not checked. What has a
     * damage threshold behind it is charge per phase: amplitude × phase width. 40 µC/cm²
     * is real and correct in exactly this form (Shannon 1992 / McCreery 1990 pulsed
     * charge-injection limits) — the figure the tree has always carried, now applied to
     * the waveform class it is actually about.
     */
    const val PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2: Double = 40.0

    /**
     * Electrode areas (cm²) for the modalities whose electrodes are FIXED PARTS OF THE
     * PRODUCT rather than a user-chosen consumable. The tDCS pad is authored per protocol
     * precisely because the user picks it (OI-CHARGE-04); the BES pads, auricular clip and
     * cervical collar are not swappable, so their area is a device property the app and
     * firmware may both hold.
     *
     * ⚠ PROVISIONAL — NOT MEASURED (OI-CHARGE-07). Mirrors
     * firmware/hub_control/include/np_hub_config.h. A SMALLER area is a SMALLER charge
     * budget, so these may be revised down freely and only revised up against a measurement.
     */
    const val BES_ELECTRODE_AREA_CM2: Double = 25.0
    const val VNS_ELECTRODE_AREA_CM2: Double = 0.5
    const val CERVICAL_VNS_ELECTRODE_AREA_CM2: Double = 2.0

    /**
     * Pulse width (seconds) the firmware substitutes when a descriptor declares none —
     * np_mod_cvns.c and np_session_runner.c both default to 250 µs. The pre-flight must
     * use the same number or it validates a different waveform than the one that runs.
     */
    const val VNS_DEFAULT_PULSE_WIDTH_SECONDS: Double = 250e-6
    /**
     * Default per-electrode pad area (cm²) for a NEWLY AUTHORED tDCS block — an editor
     * default only, not an assumption any check falls back on. 35 cm² is a standard tDCS
     * sponge pad (DHF Rev 11).
     *
     * OI-CHARGE-04 (closed 2026-09-09): the area a protocol validates against is
     * `NPTDCSParams.electrodeAreaCm2`, authored per protocol and carried in the signed
     * session descriptor, so app and safety MCU divide by the same declared number.
     * This constant had the same 35.0 as iOS and, unlike iOS, carried no note that it
     * disagreed with the enforcer's 25 cm² — the divergence was recorded at two of the
     * three app sites, and this was the third.
     */
    const val TDCS_DEFAULT_ELECTRODE_AREA_CM2: Double = 35.0
    /**
     * Largest declarable per-electrode area. Not a clinical limit — the wire field
     * (`electrode_area_mcm2`) is a uint16 of milli-cm², so 65.535 cm² is what fits.
     */
    const val TDCS_MAX_ELECTRODE_AREA_CM2: Double = 65.535
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
    /**
     * One driver channel per T2 cap electrode (NP-FW-HD-001 §6.4,
     * NP_HD_DRIVER_CHANNELS). OI-TACS-01: the hub wire mask carried only 16 bits
     * until 2026-09-07, so a protocol authoring more was silently clamped at
     * compile time; np_mod_clin_tacs_params_t now has a third mask byte.
     */
    const val CLINICAL_TACS_MAX_CHANNELS: Int = 21

    // HD-tDCS (T2) — Bikson lab safety limits for 3.5mm Ag/AgCl dual-rated electrode
    const val HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE: Double = 2.0
    const val HD_TDCS_MAX_CURRENT_DENSITY_A_M2: Double = 6.0   // ≤6 A/m² per Bikson lab limits
    const val HD_TDCS_MAX_CHARGE_DENSITY_UC_CM2: Double = 40.0 // per-phase; Class B np_hd_stim.c aborts at 95%
    /** 3.5mm Ag/AgCl sintered electrode: π × (0.175 cm)². Mirrors NP_HD_ELECTRODE_AREA_CM2. */
    const val HD_TDCS_ELECTRODE_AREA_CM2: Double = 0.0962

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
