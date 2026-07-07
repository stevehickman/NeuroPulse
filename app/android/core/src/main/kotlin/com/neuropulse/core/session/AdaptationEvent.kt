package com.neuropulse.core.session

// Port of iOS AdaptationEvent.swift — mirrors np_adapt_trigger_t / np_adaptation_event_t in
// firmware/hub_control/include/np_adaptation_log.h. One closed-loop adaptation event decoded
// from the UHDR adapt log. PRIVACY: enum-only trigger context — never any raw EEG/HRV floats.
// Plain-language copy requires Privacy Lead sign-off (OI-PA-04) before shipping.

enum class AdaptTrigger(val rawValue: Int) {
    // EEG band-power triggers
    EEG_ALPHA_LOW(0x00),
    EEG_ALPHA_HIGH(0x01),
    EEG_THETA_LOW(0x02),
    EEG_THETA_HIGH(0x03),
    EEG_GAMMA_LOW(0x04),
    EEG_GAMMA_HIGH(0x05),
    EEG_BETA_LOW(0x06),
    EEG_BETA_HIGH(0x07),
    // HRV triggers
    HRV_COHERENCE_LOW(0x08),
    HRV_COHERENCE_HIGH(0x09),
    HRV_RMSSD_LOW(0x0A),
    // Safety / device-condition triggers
    IMPEDANCE_CHANGE(0x0B),
    DOSE_LIMIT_APPROACH(0x0C),
    THERMAL_THROTTLE(0x0D),
    // Session-lifecycle triggers
    PHASE_TRANSITION(0x0E),
    USER_PAUSE(0x0F),
    SESSION_END_RAMP(0x10);

    /** Approved plain-language copy — extend only with Privacy Lead sign-off (OI-PA-04). */
    val plainLanguageDescription: String
        get() = when (this) {
            EEG_ALPHA_LOW ->
                "Brainwave activity in the calm-focus range dipped — stimulation frequency was increased to support focus."
            EEG_ALPHA_HIGH ->
                "Brainwave activity in the calm-focus range was elevated — stimulation frequency was reduced to avoid over-stimulation."
            EEG_THETA_LOW ->
                "Brainwave activity in the memory-processing range dipped — stimulation was adjusted to support memory encoding."
            EEG_THETA_HIGH ->
                "Brainwave activity in the memory-processing range was elevated — stimulation was reduced."
            EEG_GAMMA_LOW ->
                "High-frequency brainwave activity (linked to focus and attention) dipped — stimulation frequency was stepped up."
            EEG_GAMMA_HIGH ->
                "High-frequency brainwave activity was elevated — stimulation frequency was stepped down to prevent fatigue."
            EEG_BETA_LOW ->
                "Alertness-linked brainwave activity dipped — stimulation was adjusted to maintain engagement."
            EEG_BETA_HIGH ->
                "Alertness-linked brainwave activity was elevated — stimulation was reduced to encourage relaxation."
            HRV_COHERENCE_LOW ->
                "Heart rhythm coherence fell below your personal target — the breathing pacer rate was adjusted to help restore coherence."
            HRV_COHERENCE_HIGH ->
                "Heart rhythm coherence was above target — the breathing pacer rate was eased to maintain the effect without over-correction."
            HRV_RMSSD_LOW ->
                "Heart rate variability fell below your baseline — stimulation intensity was reduced to lower physiological load."
            IMPEDANCE_CHANGE ->
                "Electrode contact quality changed during the session — stimulation was adjusted to account for the change."
            DOSE_LIMIT_APPROACH ->
                "The session was approaching its cumulative light-energy limit — output was reduced to stay within the safe dose."
            THERMAL_THROTTLE ->
                "A device component reached its temperature limit — output was reduced temporarily to allow cooling."
            PHASE_TRANSITION ->
                "The session moved to a new phase — stimulation parameters were updated for the new phase."
            USER_PAUSE ->
                "The session was paused — all stimulation was reduced to standby levels."
            SESSION_END_RAMP ->
                "The session was completing — all stimulation was gradually reduced to zero."
        }

    companion object {
        fun from(rawValue: Int): AdaptTrigger? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class AdaptationEvent(
    val sessionOffsetMs: Long,
    val trigger: AdaptTrigger,
    val modType: Int,        // np_hub_mod_type_t raw value
    val paramId: Int,
    val valueBeforeX100: Int, // parameter value before × 100
    val valueAfterX100: Int,  // parameter value after × 100
    val confidencePct: Int,   // 0–100; 0xFF = not applicable
) {
    /** Stable content-derived identity (same UHDR event decoded twice → same id). */
    val id: String get() = "$sessionOffsetMs-${trigger.rawValue}-$modType-$paramId"

    val sessionOffsetSeconds: Double get() = sessionOffsetMs / 1000.0
}
