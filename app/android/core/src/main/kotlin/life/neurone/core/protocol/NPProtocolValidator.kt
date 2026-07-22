package life.neurone.core.protocol

import java.util.UUID
import kotlin.math.abs

// Port of iOS NPProtocolValidator (app/ios/NeurOne/Protocol/NPProtocolValidator.swift).
// Enforces the hardware ceilings (NPHardwareLimits) as ERRORs and the configured dosage
// limits (NPLimitsSet) as ERRORs, plus advisory WARNINGs (short/long sessions,
// photoparoxysmal risk zone, aggressive TMS, cardiac-interlock note). The error/warning
// SEMANTICS mirror iOS one-for-one; the only simplification is the limit-source attribution:
// iOS carries a per-field NPLimitSourceMap, Android attributes dosage issues to the resolved
// limits' tier (global/helmet/individual) — cosmetic only, `isValid`/errors/warnings identical.

/** Which tier a limit came from — parity with iOS NPLimitSource (display/attribution only). */
enum class NPLimitSource(val display: String) {
    HARDWARE("Hardware Limit"),
    GLOBAL("Global"),
    HELMET("Helmet"),
    INDIVIDUAL("Individual");

    companion object {
        fun forLevel(level: NPLimitsSet.LimitLevel): NPLimitSource = when (level) {
            NPLimitsSet.LimitLevel.GLOBAL -> GLOBAL
            NPLimitsSet.LimitLevel.HELMET -> HELMET
            NPLimitsSet.LimitLevel.INDIVIDUAL -> INDIVIDUAL
        }
    }
}

enum class NPValidationSeverity { ERROR, WARNING }

data class NPValidationIssue(
    val severity: NPValidationSeverity,
    val modality: NPModalityType?,          // null = protocol-level issue
    val parameterKey: String,               // camelCase field name, e.g. "intensityMilliamps"
    val parameterDisplayName: String,       // e.g. "Intensity"
    val actualValueDescription: String,     // e.g. "1.5 mA"
    val limitValueDescription: String,      // e.g. "1.0 mA (Hardware Limit)"
    val limitSource: NPLimitSource,
    val message: String,
    val id: UUID = UUID.randomUUID(),
)

class NPValidationResult {
    val issues: MutableList<NPValidationIssue> = mutableListOf()

    val isValid: Boolean get() = issues.none { it.severity == NPValidationSeverity.ERROR }
    val hasWarnings: Boolean get() = issues.any { it.severity == NPValidationSeverity.WARNING }
    val errors: List<NPValidationIssue> get() = issues.filter { it.severity == NPValidationSeverity.ERROR }
    val warnings: List<NPValidationIssue> get() = issues.filter { it.severity == NPValidationSeverity.WARNING }

    fun addError(
        modality: NPModalityType? = null,
        param: String,
        displayName: String,
        actual: String,
        limit: String,
        source: NPLimitSource,
        message: String,
    ) = issues.add(
        NPValidationIssue(
            NPValidationSeverity.ERROR, modality, param, displayName,
            actual, "$limit (${source.display})", source, message,
        ),
    )

    fun addWarning(
        modality: NPModalityType? = null,
        param: String,
        displayName: String,
        actual: String,
        limit: String,
        source: NPLimitSource,
        message: String,
    ) = issues.add(
        NPValidationIssue(
            NPValidationSeverity.WARNING, modality, param, displayName,
            actual, "$limit (${source.display})", source, message,
        ),
    )
}

/** Total session seconds if the protocol uses a duration timing mode; null for interval-count. */
val NPProtocolDefinition.totalDurationSeconds: Int?
    get() = when (val t = timingMode) {
        is NPTimingMode.Duration -> t.seconds
        is NPTimingMode.IntervalCount -> null
    }

class NPProtocolValidator(private val resolvedLimits: NPLimitsSet) {

    /** Source tier used for dosage (configured-limit) issues. */
    private val dosageSource: NPLimitSource = NPLimitSource.forLevel(resolvedLimits.level)

    // MARK: Public interface

    fun validate(definition: NPProtocolDefinition): NPValidationResult {
        val result = NPValidationResult()
        val enabled = definition.modalities.filter { it.enabled }

        if (enabled.isEmpty()) {
            result.issues.add(
                NPValidationIssue(
                    NPValidationSeverity.ERROR, null, "modalities", "Modalities",
                    "0", "≥1 (Hardware Limit)", NPLimitSource.HARDWARE,
                    "Protocol has no enabled modalities.",
                ),
            )
        }

        val dur = definition.totalDurationSeconds
        if (dur != null) {
            if (dur <= 0) {
                result.addError(
                    param = "duration", displayName = "Duration",
                    actual = "${dur}s", limit = "> 0s", source = NPLimitSource.HARDWARE,
                    message = "Session duration must be greater than zero.",
                )
            } else if (dur < 60) {
                result.addWarning(
                    param = "duration", displayName = "Duration",
                    actual = "${dur}s", limit = "60s", source = NPLimitSource.HARDWARE,
                    message = "Session is very short (< 1 minute). Verify this is intentional.",
                )
            }
            if (dur > 7200) {
                result.addWarning(
                    param = "duration", displayName = "Duration",
                    actual = "${dur / 60}m", limit = "120m", source = NPLimitSource.HARDWARE,
                    message = "Session is very long (> 2 hours). Verify this is intentional.",
                )
            }
        }

        for (block in enabled) validateModality(block, dur, result)

        // Cross-modality tDCS charge density (ISC-38): µC/cm² = I(mA) × t(s) / A(cm²).
        if (dur != null && dur > 0) {
            for (block in enabled) {
                val p = block.params
                if (p is NPModalityParams.Tdcs) {
                    val numElectrodes = p.params.electrodePairs.flatten().size.toDouble()
                    if (numElectrodes <= 0.0) continue
                    val totalArea = NPHardwareLimits.TDCS_DEFAULT_ELECTRODE_AREA_CM2 * numElectrodes
                    val chargeDensity = p.params.intensityMilliamps * dur.toDouble() / totalArea
                    if (chargeDensity > NPHardwareLimits.TDCS_MAX_CHARGE_DENSITY_UC_CM2) {
                        result.addError(
                            modality = NPModalityType.TDCS,
                            param = "chargeDensityUCcm2", displayName = "Charge Density",
                            actual = fmt1(chargeDensity) + " µC/cm²",
                            limit = "${NPHardwareLimits.TDCS_MAX_CHARGE_DENSITY_UC_CM2.toInt()} µC/cm²",
                            source = NPLimitSource.HARDWARE,
                            message = "Estimated tDCS charge density ${fmt1(chargeDensity)} µC/cm² " +
                                "exceeds the 40 µC/cm² safety ceiling. Reduce current or session duration.",
                        )
                    }
                }
            }
        }

        val hasBES = enabled.any { it.params is NPModalityParams.BesTacs }
        val hasTDCS = enabled.any { it.params is NPModalityParams.Tdcs }
        if (hasBES && hasTDCS) {
            result.addWarning(
                param = "cross_modality", displayName = "Cross-modality",
                actual = "BES + tDCS active", limit = "Separate electrode paths",
                source = NPLimitSource.HARDWARE,
                message = "BES and tDCS active simultaneously — ensure electrode paths are " +
                    "non-overlapping to prevent current interaction.",
            )
        }

        return result
    }

    /**
     * Validate any entry. Composites resolve their member protocols via [resolveSingle]
     * (parity with iOS `validate(_:resolving:)`); return null for an unknown name.
     */
    fun validate(
        entry: NPProtocolEntry,
        resolveSingle: (String) -> NPProtocolDefinition? = { null },
    ): NPValidationResult = when (entry) {
        is NPProtocolEntry.Single -> validate(entry.protocol)
        is NPProtocolEntry.Composite -> validateComposite(entry.composite, resolveSingle)
        is NPProtocolEntry.Limits -> NPValidationResult()
    }

    // MARK: Composite

    private fun validateComposite(
        c: NPCompositeProtocol,
        resolveSingle: (String) -> NPProtocolDefinition?,
    ): NPValidationResult {
        val result = NPValidationResult()
        if (c.layers.isEmpty()) {
            result.issues.add(
                NPValidationIssue(
                    NPValidationSeverity.ERROR, null, "layers", "Layers",
                    "0", "≥1 (Hardware Limit)", NPLimitSource.HARDWARE,
                    "Composite protocol has no layers.",
                ),
            )
        }
        for (layer in c.layers) {
            val def = resolveSingle(layer.protocolName)
            if (def != null) {
                val sub = validate(def)
                for (issue in sub.issues) {
                    result.issues.add(issue.copy(message = "[${layer.protocolName}] ${issue.message}"))
                }
            } else {
                result.issues.add(
                    NPValidationIssue(
                        NPValidationSeverity.ERROR, null, "layer_ref", "Layer",
                        layer.protocolName, "Known protocol (Hardware Limit)", NPLimitSource.HARDWARE,
                        "Layer references unknown protocol '${layer.protocolName}'.",
                    ),
                )
            }
        }
        return result
    }

    // MARK: Per-modality dispatch

    private fun validateModality(block: NPProtocolModality, totalDurationSeconds: Int?, result: NPValidationResult) {
        when (val p = block.params) {
            is NPModalityParams.PbmTranscranial -> validatePBMTranscranial(p.params, totalDurationSeconds, result)
            is NPModalityParams.PbmIntranasal -> validatePBMIntranasal(p.params, block.interval, result)
            is NPModalityParams.EegNeurofeedback -> validateEEG(p.params, result)
            is NPModalityParams.BesTacs -> validateBESTacs(p.params, block.interval, result)
            is NPModalityParams.Tdcs -> validateTDCS(p.params, block.interval, result)
            is NPModalityParams.VnsHRV -> validateVNS(p.params, block.interval, result)
            is NPModalityParams.AudioEntrainment -> validateAudio(p.params, result)
            is NPModalityParams.VisualStimulation -> validateVisual(p.params, result)
            is NPModalityParams.Qeeg21ch -> Unit // passive recording — no stimulation limits
            is NPModalityParams.Tms -> validateTMS(p.params, result)
            is NPModalityParams.PbmDeep1170nm -> validateDeepPBM(p.params, block.interval, result)
            is NPModalityParams.ClinicalTacs -> validateClinicalTacs(p.params, block.interval, result)
            is NPModalityParams.HdTdcs -> validateHDTdcs(p.params, block.interval, result)
            is NPModalityParams.CervicalVns -> validateCervicalVns(p.params, block.interval, result)
            is NPModalityParams.Vibrotactile40hz -> validateVibrotactile(p.params, result)
        }
    }

    // MARK: Per-modality validators

    private fun validatePBMTranscranial(p: NPPBMTranscranialParams, totalDurationSeconds: Int?, r: NPValidationResult) {
        val m = NPModalityType.PBM_TRANSCRANIAL
        val lim = resolvedLimits.pbmTranscranial

        if (p.dutyCyclePercent > NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT) {
            r.addError(m, "dutyCyclePercent", "Duty Cycle", "${p.dutyCyclePercent}%",
                "${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%", NPLimitSource.HARDWARE,
                "PBM duty cycle ${p.dutyCyclePercent}% exceeds firmware-enforced maximum of " +
                    "${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%.")
        }
        if (p.frequencyHz < 0) {
            r.addError(m, "frequencyHz", "Frequency", "${p.frequencyHz} Hz", "≥0 Hz",
                NPLimitSource.HARDWARE, "PBM frequency cannot be negative.")
        }
        lim?.maxIntensityPercent?.let { maxI ->
            if (p.intensityPercent > maxI) r.addError(m, "intensityPercent", "Intensity",
                "${p.intensityPercent.toInt()}%", "${maxI.toInt()}%", dosageSource,
                "PBM transcranial intensity ${p.intensityPercent.toInt()}% exceeds limit of ${maxI.toInt()}%.")
        }
        lim?.maxFrequencyHz?.let { maxF ->
            if (p.frequencyHz > maxF) r.addError(m, "frequencyHz", "Frequency",
                fmtHz(p.frequencyHz), fmtHz(maxF), dosageSource,
                "PBM frequency ${fmtHz(p.frequencyHz)} exceeds limit of ${fmtHz(maxF)}.")
        }
        lim?.maxDutyCyclePercent?.let { maxDC ->
            if (p.dutyCyclePercent > maxDC) r.addError(m, "dutyCyclePercent", "Duty Cycle",
                "${p.dutyCyclePercent}%", "$maxDC%", dosageSource,
                "PBM duty cycle ${p.dutyCyclePercent}% exceeds configured limit of $maxDC%.")
        }
        val maxDose = lim?.maxSessionDoseJCm2
        if (maxDose != null && totalDurationSeconds != null && totalDurationSeconds > 0) {
            val peakMWcm2 = if (p.frequencyHz == 0.0) NPHardwareLimits.PBM_CW_MAX_MW_CM2
                else NPHardwareLimits.PBM_PULSED_PEAK_MW_CM2 * p.dutyCyclePercent / 100.0
            val estimatedDose = peakMWcm2 * (p.intensityPercent / 100.0) * totalDurationSeconds / 1000.0
            if (estimatedDose > maxDose) r.addError(m, "sessionDoseJCm2", "Session Dose",
                fmt1(estimatedDose) + " J/cm²", fmt1(maxDose) + " J/cm²", dosageSource,
                "Estimated PBM session dose ${fmt1(estimatedDose)} J/cm² exceeds the configured " +
                    "limit of ${fmt1(maxDose)} J/cm².")
        }
    }

    private fun validatePBMIntranasal(p: NPPBMIntranasalParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.PBM_INTRANASAL
        val lim = resolvedLimits.pbmIntranasal
        if (p.dutyCyclePercent > NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT) {
            r.addError(m, "dutyCyclePercent", "Duty Cycle", "${p.dutyCyclePercent}%",
                "${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%", NPLimitSource.HARDWARE,
                "Intranasal PBM duty cycle ${p.dutyCyclePercent}% exceeds firmware-enforced maximum of " +
                    "${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%.")
        }
        lim?.maxIntensityPercent?.let { maxI ->
            if (p.intensityPercent > maxI) r.addError(m, "intensityPercent", "Intensity",
                "${p.intensityPercent.toInt()}%", "${maxI.toInt()}%", dosageSource,
                "Intranasal PBM intensity ${p.intensityPercent.toInt()}% exceeds limit of ${maxI.toInt()}%.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "Intranasal PBM session duration ${fmtSec(interval.intervalOnSeconds)} " +
                    "exceeds limit of ${fmtSec(maxDur)}.")
        }
    }

    private fun validateEEG(p: NPEEGNeurofeedbackParams, r: NPValidationResult) {
        val m = NPModalityType.EEG_NEUROFEEDBACK
        val lim = resolvedLimits.eegNeurofeedback
        lim?.allowedBands?.let { allowed ->
            if (!allowed.contains(p.band.rawValue)) r.addError(m, "band", "EEG Band",
                p.band.rawValue, allowed.joinToString(", "), dosageSource,
                "EEG band '${p.band.rawValue}' is not in the allowed list: ${allowed.joinToString(", ")}.")
        }
        lim?.requireClosedLoop?.let { requireCL ->
            if (requireCL && !p.closedLoopEnabled) r.addError(m, "closedLoopEnabled", "Closed Loop",
                "disabled", "required", dosageSource,
                "EEG neurofeedback requires closed-loop mode to be enabled per current limits.")
        }
    }

    private fun validateBESTacs(p: NPBESTacsParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.BES_TACS
        val lim = resolvedLimits.besTacs
        if (p.intensityMilliamps > NPHardwareLimits.BES_TACS_MAX_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.BES_TACS_MAX_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "BES/tACS intensity ${p.intensityMilliamps} mA exceeds firmware-enforced maximum of " +
                "${NPHardwareLimits.BES_TACS_MAX_MILLIAMPS} mA.")
        if (p.frequencyHz < NPHardwareLimits.BES_TACS_MIN_HZ) r.addError(m, "frequencyHz", "Frequency",
            fmtHz(p.frequencyHz), "≥${fmtHz(NPHardwareLimits.BES_TACS_MIN_HZ)}", NPLimitSource.HARDWARE,
            "BES/tACS frequency ${fmtHz(p.frequencyHz)} is below minimum of ${fmtHz(NPHardwareLimits.BES_TACS_MIN_HZ)}.")
        if (p.frequencyHz > NPHardwareLimits.BES_TACS_MAX_HZ) r.addError(m, "frequencyHz", "Frequency",
            fmtHz(p.frequencyHz), "≤${fmtHz(NPHardwareLimits.BES_TACS_MAX_HZ)}", NPLimitSource.HARDWARE,
            "BES/tACS frequency ${fmtHz(p.frequencyHz)} exceeds firmware-enforced maximum of " +
                "${fmtHz(NPHardwareLimits.BES_TACS_MAX_HZ)}.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "BES/tACS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxFrequencyHz?.let { maxF ->
            if (p.frequencyHz > maxF) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                fmtHz(maxF), dosageSource, "BES/tACS frequency ${fmtHz(p.frequencyHz)} exceeds limit of ${fmtHz(maxF)}.")
        }
        lim?.minFrequencyHz?.let { minF ->
            if (p.frequencyHz < minF) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                "≥${fmtHz(minF)}", dosageSource, "BES/tACS frequency ${fmtHz(p.frequencyHz)} is below limit of ${fmtHz(minF)}.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "BES/tACS interval duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
    }

    private fun validateTDCS(p: NPTDCSParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.TDCS
        val lim = resolvedLimits.tdcs
        if (p.intensityMilliamps < NPHardwareLimits.TDCS_MIN_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "≥${NPHardwareLimits.TDCS_MIN_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "tDCS intensity ${p.intensityMilliamps} mA is below the minimum of ${NPHardwareLimits.TDCS_MIN_MILLIAMPS} mA.")
        if (p.intensityMilliamps > NPHardwareLimits.TDCS_MAX_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.TDCS_MAX_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "tDCS intensity ${p.intensityMilliamps} mA exceeds firmware-enforced maximum of ${NPHardwareLimits.TDCS_MAX_MILLIAMPS} mA.")
        if (p.electrodePairs.size > NPHardwareLimits.TDCS_MAX_ELECTRODE_PAIRS) r.addError(m,
            "electrodePairs", "Electrode Pairs", "${p.electrodePairs.size}",
            "${NPHardwareLimits.TDCS_MAX_ELECTRODE_PAIRS}", NPLimitSource.HARDWARE,
            "tDCS has ${p.electrodePairs.size} electrode pairs, exceeding the maximum of ${NPHardwareLimits.TDCS_MAX_ELECTRODE_PAIRS}.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "tDCS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "tDCS session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
    }

    private fun validateVNS(p: NPVNSHRVParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.VNS_HRV
        val lim = resolvedLimits.vnsHrv
        if (p.intensityMilliamps > NPHardwareLimits.VNS_MAX_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.VNS_MAX_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "VNS intensity ${p.intensityMilliamps} mA exceeds firmware-enforced maximum of ${NPHardwareLimits.VNS_MAX_MILLIAMPS} mA.")
        if (p.frequencyHz < NPHardwareLimits.VNS_MIN_HZ) r.addError(m, "frequencyHz", "Frequency",
            fmtHz(p.frequencyHz), "≥${fmtHz(NPHardwareLimits.VNS_MIN_HZ)}", NPLimitSource.HARDWARE,
            "VNS frequency ${fmtHz(p.frequencyHz)} is below minimum of ${fmtHz(NPHardwareLimits.VNS_MIN_HZ)}.")
        if (p.frequencyHz > NPHardwareLimits.VNS_MAX_HZ) r.addError(m, "frequencyHz", "Frequency",
            fmtHz(p.frequencyHz), "≤${fmtHz(NPHardwareLimits.VNS_MAX_HZ)}", NPLimitSource.HARDWARE,
            "VNS frequency ${fmtHz(p.frequencyHz)} exceeds firmware-enforced maximum of ${fmtHz(NPHardwareLimits.VNS_MAX_HZ)}.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "VNS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxFrequencyHz?.let { maxF ->
            if (p.frequencyHz > maxF) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                fmtHz(maxF), dosageSource, "VNS frequency ${fmtHz(p.frequencyHz)} exceeds limit of ${fmtHz(maxF)}.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "VNS session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
        lim?.allowedProtocols?.let { allowed ->
            if (!allowed.contains(p.hrvProtocol.rawValue)) r.addError(m, "hrvProtocol", "HRV Protocol",
                p.hrvProtocol.rawValue, allowed.joinToString(", "), dosageSource,
                "HRV protocol '${p.hrvProtocol.rawValue}' is not in the allowed list.")
        }
    }

    private fun validateAudio(p: NPAudioEntrainmentParams, r: NPValidationResult) {
        val m = NPModalityType.AUDIO_ENTRAINMENT
        val lim = resolvedLimits.audioEntrainment
        lim?.maxVolumePercent?.let { maxVol ->
            if (p.volumePercent > maxVol) r.addError(m, "volumePercent", "Volume",
                "${p.volumePercent.toInt()}%", "${maxVol.toInt()}%", dosageSource,
                "Audio volume ${p.volumePercent.toInt()}% exceeds limit of ${maxVol.toInt()}%.")
        }
        val bb = p.binauralBeatsHz
        lim?.maxBinauralBeatsHz?.let { maxBB ->
            if (bb != null && bb > maxBB) r.addError(m, "binauralBeatsHz", "Binaural Beats",
                fmtHz(bb), fmtHz(maxBB), dosageSource,
                "Binaural beats ${fmtHz(bb)} exceeds limit of ${fmtHz(maxBB)}.")
        }
        val it = p.isochronicTonesHz
        lim?.maxIsochronicTonesHz?.let { maxIT ->
            if (it != null && it > maxIT) r.addError(m, "isochronicTonesHz", "Isochronic Tones",
                fmtHz(it), fmtHz(maxIT), dosageSource,
                "Isochronic tones ${fmtHz(it)} exceeds limit of ${fmtHz(maxIT)}.")
        }
    }

    private fun validateVisual(p: NPVisualStimParams, r: NPValidationResult) {
        val m = NPModalityType.VISUAL_STIMULATION
        val lim = resolvedLimits.visualStimulation
        if (p.frequencyHz > NPHardwareLimits.VISUAL_MAX_HZ) r.addError(m, "frequencyHz", "Frequency",
            fmtHz(p.frequencyHz), fmtHz(NPHardwareLimits.VISUAL_MAX_HZ), NPLimitSource.HARDWARE,
            "Visual stimulation frequency ${fmtHz(p.frequencyHz)} exceeds firmware-enforced maximum of ${fmtHz(NPHardwareLimits.VISUAL_MAX_HZ)}.")
        if (p.frequencyHz >= NPHardwareLimits.VISUAL_HIGH_RISK_MIN_HZ && p.frequencyHz <= NPHardwareLimits.VISUAL_HIGH_RISK_MAX_HZ) {
            val blockRange = lim?.blockHighRiskRange ?: false
            val msg = "Visual stimulation at ${fmtHz(p.frequencyHz)} is in the photoparoxysmal risk zone " +
                "(${NPHardwareLimits.VISUAL_HIGH_RISK_MIN_HZ.toInt()}–${NPHardwareLimits.VISUAL_HIGH_RISK_MAX_HZ.toInt()} Hz). " +
                "Clinician unlock required for this range per device safety policy."
            val limitDesc = "Outside ${NPHardwareLimits.VISUAL_HIGH_RISK_MIN_HZ.toInt()}–${NPHardwareLimits.VISUAL_HIGH_RISK_MAX_HZ.toInt()} Hz"
            if (blockRange) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz), limitDesc, dosageSource, msg)
            else r.addWarning(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz), limitDesc, NPLimitSource.HARDWARE, msg)
        }
        lim?.maxFrequencyHz?.let { maxF ->
            if (p.frequencyHz > maxF) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                fmtHz(maxF), dosageSource, "Visual stimulation frequency ${fmtHz(p.frequencyHz)} exceeds limit of ${fmtHz(maxF)}.")
        }
        lim?.minFrequencyHz?.let { minF ->
            if (p.frequencyHz < minF) r.addError(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                "≥${fmtHz(minF)}", dosageSource, "Visual stimulation frequency ${fmtHz(p.frequencyHz)} is below limit of ${fmtHz(minF)}.")
        }
        lim?.allowedModes?.let { allowed ->
            if (!allowed.contains(p.mode.rawValue)) r.addError(m, "mode", "Visual Mode",
                p.mode.rawValue, allowed.joinToString(", "), dosageSource,
                "Visual mode '${p.mode.rawValue}' is not in the allowed list.")
        }
    }

    private fun validateTMS(p: NPTMSParams, r: NPValidationResult) {
        val m = NPModalityType.TMS
        val lim = resolvedLimits.tms
        lim?.maxIntensityPercentMT?.let { maxMT ->
            if (p.intensityPercentMT > maxMT) r.addError(m, "intensityPercentMT", "Intensity (%MT)",
                "${p.intensityPercentMT}% MT", "$maxMT% MT", dosageSource,
                "TMS intensity ${p.intensityPercentMT}% MT exceeds limit of $maxMT% MT.")
        }
        lim?.maxPulsesPerSession?.let { maxPulses ->
            if (p.pulseCount > maxPulses) r.addError(m, "pulseCount", "Pulse Count",
                "${p.pulseCount} pulses", "$maxPulses pulses", dosageSource,
                "TMS pulse count ${p.pulseCount} exceeds session limit of $maxPulses pulses.")
        }
        lim?.allowedProtocols?.let { allowed ->
            if (!allowed.contains(p.tmsProtocol.rawValue)) r.addError(m, "tmsProtocol", "TMS Protocol",
                p.tmsProtocol.rawValue, allowed.joinToString(", "), dosageSource,
                "TMS protocol '${p.tmsProtocol.rawValue}' is not in the allowed list.")
        }
        lim?.allowedTargets?.let { allowed ->
            if (!allowed.contains(p.target.rawValue)) r.addError(m, "target", "TMS Target",
                p.target.rawValue, allowed.joinToString(", "), dosageSource,
                "TMS target '${p.target.rawValue}' is not in the allowed list.")
        }
        if (p.intensityPercentMT > 120) r.addWarning(m, "intensityPercentMT", "Intensity (%MT)",
            "${p.intensityPercentMT}% MT", "≤120% MT", NPLimitSource.HARDWARE,
            "TMS intensity ${p.intensityPercentMT}% MT is high (>120% MT). Verify this is prescribed.")
    }

    private fun validateDeepPBM(p: NPDeepPBM1170Params, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.PBM_DEEP_1170NM
        val lim = resolvedLimits.pbmDeep1170nm
        if (p.intensityMWcm2 > NPHardwareLimits.DEEP_PBM_MAX_MW_CM2) r.addError(m, "intensityMWcm2", "Intensity",
            "${p.intensityMWcm2.toInt()} mW/cm²", "${NPHardwareLimits.DEEP_PBM_MAX_MW_CM2.toInt()} mW/cm²",
            NPLimitSource.HARDWARE, "Deep PBM 1170nm intensity ${p.intensityMWcm2.toInt()} mW/cm² exceeds maximum of " +
                "${NPHardwareLimits.DEEP_PBM_MAX_MW_CM2.toInt()} mW/cm².")
        if (p.dutyCyclePercent > NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT) r.addError(m, "dutyCyclePercent", "Duty Cycle",
            "${p.dutyCyclePercent}%", "${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%", NPLimitSource.HARDWARE,
            "Deep PBM duty cycle ${p.dutyCyclePercent}% exceeds firmware-enforced maximum of ${NPHardwareLimits.PBM_DUTY_CYCLE_MAX_PERCENT}%.")
        lim?.maxIntensityMWcm2?.let { maxI ->
            if (p.intensityMWcm2 > maxI) r.addError(m, "intensityMWcm2", "Intensity",
                "${p.intensityMWcm2.toInt()} mW/cm²", "${maxI.toInt()} mW/cm²", dosageSource,
                "Deep PBM 1170nm intensity ${p.intensityMWcm2.toInt()} mW/cm² exceeds limit of ${maxI.toInt()} mW/cm².")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "Deep PBM session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
    }

    private fun validateClinicalTacs(p: NPClinicalTacsParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.CLINICAL_TACS
        val lim = resolvedLimits.clinicalTacs
        if (p.intensityMilliamps > NPHardwareLimits.CLINICAL_TACS_MAX_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.CLINICAL_TACS_MAX_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "Clinical tACS intensity ${p.intensityMilliamps} mA exceeds firmware-enforced maximum of ${NPHardwareLimits.CLINICAL_TACS_MAX_MILLIAMPS} mA.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "Clinical tACS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "Clinical tACS session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
    }

    private fun validateHDTdcs(p: NPHDTdcsParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.HD_TDCS
        val lim = resolvedLimits.hdTdcs
        if (p.intensityMilliamps > NPHardwareLimits.HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE} mA", NPLimitSource.HARDWARE,
            "HD-tDCS intensity ${p.intensityMilliamps} mA/electrode exceeds Bikson lab safety limit of " +
                "${NPHardwareLimits.HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE} mA/electrode for 3.5mm Ag/AgCl electrodes.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "HD-tDCS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "HD-tDCS session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
        lim?.allowedMontages?.let { allowed ->
            if (!allowed.contains(p.montage.rawValue)) r.addError(m, "montage", "Montage",
                p.montage.rawValue, allowed.joinToString(", "), dosageSource,
                "HD-tDCS montage '${p.montage.rawValue}' is not in the allowed list.")
        }
    }

    private fun validateCervicalVns(p: NPCervicalVnsParams, interval: NPIntervalConfig, r: NPValidationResult) {
        val m = NPModalityType.CERVICAL_VNS
        val lim = resolvedLimits.cervicalVns
        if (p.intensityMilliamps > NPHardwareLimits.CERVICAL_VNS_MAX_MILLIAMPS) r.addError(m,
            "intensityMilliamps", "Intensity", "${p.intensityMilliamps} mA",
            "${NPHardwareLimits.CERVICAL_VNS_MAX_MILLIAMPS} mA", NPLimitSource.HARDWARE,
            "Cervical VNS intensity ${p.intensityMilliamps} mA exceeds firmware-enforced maximum of " +
                "${NPHardwareLimits.CERVICAL_VNS_MAX_MILLIAMPS} mA. Cardiac interlock is always enforced by safety MCU regardless.")
        if (p.frequencyHz < NPHardwareLimits.VNS_MIN_HZ || p.frequencyHz > NPHardwareLimits.VNS_MAX_HZ) r.addError(m,
            "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
            "${fmtHz(NPHardwareLimits.VNS_MIN_HZ)}–${fmtHz(NPHardwareLimits.VNS_MAX_HZ)}", NPLimitSource.HARDWARE,
            "Cervical VNS frequency ${fmtHz(p.frequencyHz)} is outside the valid range of " +
                "${fmtHz(NPHardwareLimits.VNS_MIN_HZ)}–${fmtHz(NPHardwareLimits.VNS_MAX_HZ)}.")
        lim?.maxIntensityMilliamps?.let { maxI ->
            if (p.intensityMilliamps > maxI) r.addError(m, "intensityMilliamps", "Intensity",
                "${p.intensityMilliamps} mA", "$maxI mA", dosageSource,
                "Cervical VNS intensity ${p.intensityMilliamps} mA exceeds limit of $maxI mA.")
        }
        lim?.maxSessionDurationSeconds?.let { maxDur ->
            if (!interval.isContinuous && interval.intervalOnSeconds > maxDur) r.addError(m,
                "sessionDuration", "Session Duration", fmtSec(interval.intervalOnSeconds), fmtSec(maxDur),
                dosageSource, "Cervical VNS session duration ${fmtSec(interval.intervalOnSeconds)} exceeds limit of ${fmtSec(maxDur)}.")
        }
        r.addWarning(m, "cardiacInterlock", "Cardiac Interlock", "always on", "non-overridable",
            NPLimitSource.HARDWARE, "Cervical VNS cardiac rhythm interlock is always enforced by the safety MCU. " +
                "HR change >15 BPM within 5s will automatically stop stimulation.")
    }

    private fun validateVibrotactile(p: NPVibrotactileParams, r: NPValidationResult) {
        val m = NPModalityType.VIBROTACTILE_40HZ
        val lim = resolvedLimits.vibrotactile40hz
        if (p.intensityG < NPHardwareLimits.VIBROTACTILE_MIN_G) r.addError(m, "intensityG", "Intensity",
            "${p.intensityG} G", "≥${NPHardwareLimits.VIBROTACTILE_MIN_G} G", NPLimitSource.HARDWARE,
            "Vibrotactile intensity ${p.intensityG} G is below the minimum of ${NPHardwareLimits.VIBROTACTILE_MIN_G} G for effective entrainment.")
        if (p.intensityG > NPHardwareLimits.VIBROTACTILE_MAX_G) r.addError(m, "intensityG", "Intensity",
            "${p.intensityG} G", "${NPHardwareLimits.VIBROTACTILE_MAX_G} G", NPLimitSource.HARDWARE,
            "Vibrotactile intensity ${p.intensityG} G exceeds DRV2605L driver maximum of ${NPHardwareLimits.VIBROTACTILE_MAX_G} G.")
        if (abs(p.frequencyHz - NPHardwareLimits.VIBROTACTILE_FREQUENCY_HZ) > NPHardwareLimits.VIBROTACTILE_FREQ_TOLERANCE_HZ) {
            r.addWarning(m, "frequencyHz", "Frequency", fmtHz(p.frequencyHz),
                "${NPHardwareLimits.VIBROTACTILE_FREQUENCY_HZ.toInt()} Hz ±${NPHardwareLimits.VIBROTACTILE_FREQ_TOLERANCE_HZ} Hz",
                NPLimitSource.HARDWARE, "Vibrotactile frequency ${fmtHz(p.frequencyHz)} deviates from the " +
                    "firmware-locked 40 Hz ± 0.5 Hz target. Firmware will lock to 40 Hz regardless.")
        }
        lim?.maxIntensityG?.let { maxG ->
            if (p.intensityG > maxG) r.addError(m, "intensityG", "Intensity", "${p.intensityG} G",
                "$maxG G", dosageSource, "Vibrotactile intensity ${p.intensityG} G exceeds limit of $maxG G.")
        }
    }

    // MARK: Formatting helpers

    private fun fmtHz(hz: Double): String =
        if (hz == hz.toInt().toDouble()) "${hz.toInt()} Hz" else "$hz Hz"

    private fun fmtSec(s: Int): String {
        if (s < 60) return "${s}s"
        val mm = s / 60
        val sec = s % 60
        return if (sec == 0) "${mm}m" else "${mm}m ${sec}s"
    }

    private fun fmt1(v: Double): String = String.format("%.1f", v)
}
