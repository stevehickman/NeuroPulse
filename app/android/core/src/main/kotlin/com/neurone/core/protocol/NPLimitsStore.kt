package com.neurone.core.protocol

import com.neurone.core.common.KeyValueStore
import java.util.UUID

// Port of iOS NPLimitsStore + NPLimitsSet.resolve (app/ios/NeurOne/Protocol/NPLimitsStore.swift,
// NPDosageLimits.swift). Manages dosage limit tiers — global / per-helmet / per-individual —
// and resolves them field-by-field (individual > helmet > global) into the effective NPLimitsSet
// the validator enforces. The per-field NPLimitSourceMap (cosmetic attribution) is intentionally
// dropped on Android — the validator attributes dosage issues to the resolved set's tier.

data class NPIndividualProfile(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val notes: String = "",
)

/** individual ?? helmet ?? global for a single optional field. */
private fun <T> pick(i: T?, h: T?, g: T?): T? = i ?: h ?: g

/**
 * Field-by-field resolution of the three tiers into one effective NPLimitsSet.
 * A modality block is null (unlimited) only when all three tiers omit it.
 */
fun resolveLimits(global: NPLimitsSet?, helmet: NPLimitsSet?, individual: NPLimitsSet?): NPLimitsSet {
    fun <A> merge(i: A?, h: A?, g: A?, build: (A?, A?, A?) -> A?): A? =
        if (i == null && h == null && g == null) null else build(i, h, g)

    return NPLimitsSet(name = "Resolved", level = NPLimitsSet.LimitLevel.GLOBAL).apply {
        pbmTranscranial = merge(individual?.pbmTranscranial, helmet?.pbmTranscranial, global?.pbmTranscranial) { i, h, g ->
            NPPBMTranscranialLimits(
                maxIntensityPercent = pick(i?.maxIntensityPercent, h?.maxIntensityPercent, g?.maxIntensityPercent),
                maxFrequencyHz = pick(i?.maxFrequencyHz, h?.maxFrequencyHz, g?.maxFrequencyHz),
                maxDutyCyclePercent = pick(i?.maxDutyCyclePercent, h?.maxDutyCyclePercent, g?.maxDutyCyclePercent),
                maxSessionDoseJCm2 = pick(i?.maxSessionDoseJCm2, h?.maxSessionDoseJCm2, g?.maxSessionDoseJCm2),
                maxDailyDoseJCm2 = pick(i?.maxDailyDoseJCm2, h?.maxDailyDoseJCm2, g?.maxDailyDoseJCm2),
            )
        }
        pbmIntranasal = merge(individual?.pbmIntranasal, helmet?.pbmIntranasal, global?.pbmIntranasal) { i, h, g ->
            NPPBMIntranasalLimits(
                maxIntensityPercent = pick(i?.maxIntensityPercent, h?.maxIntensityPercent, g?.maxIntensityPercent),
                maxSessionDoseJCm2 = pick(i?.maxSessionDoseJCm2, h?.maxSessionDoseJCm2, g?.maxSessionDoseJCm2),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            )
        }
        eegNeurofeedback = merge(individual?.eegNeurofeedback, helmet?.eegNeurofeedback, global?.eegNeurofeedback) { i, h, g ->
            NPEEGNeurofeedbackLimits(
                allowedBands = pick(i?.allowedBands, h?.allowedBands, g?.allowedBands),
                requireClosedLoop = pick(i?.requireClosedLoop, h?.requireClosedLoop, g?.requireClosedLoop),
            )
        }
        besTacs = merge(individual?.besTacs, helmet?.besTacs, global?.besTacs) { i, h, g ->
            NPBESTacsLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxFrequencyHz = pick(i?.maxFrequencyHz, h?.maxFrequencyHz, g?.maxFrequencyHz),
                minFrequencyHz = pick(i?.minFrequencyHz, h?.minFrequencyHz, g?.minFrequencyHz),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
                maxSessionsPerDay = pick(i?.maxSessionsPerDay, h?.maxSessionsPerDay, g?.maxSessionsPerDay),
            )
        }
        tdcs = merge(individual?.tdcs, helmet?.tdcs, global?.tdcs) { i, h, g ->
            NPTDCSLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
                maxSessionsPerDay = pick(i?.maxSessionsPerDay, h?.maxSessionsPerDay, g?.maxSessionsPerDay),
            )
        }
        vnsHrv = merge(individual?.vnsHrv, helmet?.vnsHrv, global?.vnsHrv) { i, h, g ->
            NPVNSHRVLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxFrequencyHz = pick(i?.maxFrequencyHz, h?.maxFrequencyHz, g?.maxFrequencyHz),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
                allowedProtocols = pick(i?.allowedProtocols, h?.allowedProtocols, g?.allowedProtocols),
            )
        }
        audioEntrainment = merge(individual?.audioEntrainment, helmet?.audioEntrainment, global?.audioEntrainment) { i, h, g ->
            NPAudioEntrainmentLimits(
                maxVolumePercent = pick(i?.maxVolumePercent, h?.maxVolumePercent, g?.maxVolumePercent),
                maxBinauralBeatsHz = pick(i?.maxBinauralBeatsHz, h?.maxBinauralBeatsHz, g?.maxBinauralBeatsHz),
                maxIsochronicTonesHz = pick(i?.maxIsochronicTonesHz, h?.maxIsochronicTonesHz, g?.maxIsochronicTonesHz),
            )
        }
        visualStimulation = merge(individual?.visualStimulation, helmet?.visualStimulation, global?.visualStimulation) { i, h, g ->
            NPVisualStimLimits(
                maxFrequencyHz = pick(i?.maxFrequencyHz, h?.maxFrequencyHz, g?.maxFrequencyHz),
                minFrequencyHz = pick(i?.minFrequencyHz, h?.minFrequencyHz, g?.minFrequencyHz),
                allowedModes = pick(i?.allowedModes, h?.allowedModes, g?.allowedModes),
                blockHighRiskRange = pick(i?.blockHighRiskRange, h?.blockHighRiskRange, g?.blockHighRiskRange),
            )
        }
        tms = merge(individual?.tms, helmet?.tms, global?.tms) { i, h, g ->
            NPTMSLimits(
                maxIntensityPercentMT = pick(i?.maxIntensityPercentMT, h?.maxIntensityPercentMT, g?.maxIntensityPercentMT),
                maxPulsesPerSession = pick(i?.maxPulsesPerSession, h?.maxPulsesPerSession, g?.maxPulsesPerSession),
                maxPulsesPerDay = pick(i?.maxPulsesPerDay, h?.maxPulsesPerDay, g?.maxPulsesPerDay),
                maxSessionsPerWeek = pick(i?.maxSessionsPerWeek, h?.maxSessionsPerWeek, g?.maxSessionsPerWeek),
                allowedProtocols = pick(i?.allowedProtocols, h?.allowedProtocols, g?.allowedProtocols),
                allowedTargets = pick(i?.allowedTargets, h?.allowedTargets, g?.allowedTargets),
            )
        }
        pbmDeep1170nm = merge(individual?.pbmDeep1170nm, helmet?.pbmDeep1170nm, global?.pbmDeep1170nm) { i, h, g ->
            NPDeepPBMLimits(
                maxIntensityMWcm2 = pick(i?.maxIntensityMWcm2, h?.maxIntensityMWcm2, g?.maxIntensityMWcm2),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            )
        }
        clinicalTacs = merge(individual?.clinicalTacs, helmet?.clinicalTacs, global?.clinicalTacs) { i, h, g ->
            NPClinicalTacsLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            )
        }
        hdTdcs = merge(individual?.hdTdcs, helmet?.hdTdcs, global?.hdTdcs) { i, h, g ->
            NPHDTdcsLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
                allowedMontages = pick(i?.allowedMontages, h?.allowedMontages, g?.allowedMontages),
            )
        }
        cervicalVns = merge(individual?.cervicalVns, helmet?.cervicalVns, global?.cervicalVns) { i, h, g ->
            NPCervicalVnsLimits(
                maxIntensityMilliamps = pick(i?.maxIntensityMilliamps, h?.maxIntensityMilliamps, g?.maxIntensityMilliamps),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            )
        }
        vibrotactile40hz = merge(individual?.vibrotactile40hz, helmet?.vibrotactile40hz, global?.vibrotactile40hz) { i, h, g ->
            NPVibrotactileLimits(
                maxIntensityG = pick(i?.maxIntensityG, h?.maxIntensityG, g?.maxIntensityG),
                maxSessionDurationSeconds = pick(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            )
        }
    }
}

class NPLimitsStore(private val kv: KeyValueStore) {

    companion object {
        const val GLOBAL_KEY = "np.limits.global"
    }

    var globalLimits: NPLimitsSet? = null
        private set

    // Helmet/individual tiers + profiles are in-memory for now (global is the editable/persisted
    // tier). Persisting the maps + profiles is a follow-up (OI-AND-LIMITS-01).
    var helmetLimits: Map<String, NPLimitsSet> = emptyMap()
        private set
    var individualLimits: Map<String, NPLimitsSet> = emptyMap()
        private set
    var profiles: List<NPIndividualProfile> = emptyList()
        private set
    var activeHelmetSerial: String? = null
    var activeProfileId: String? = null

    init {
        loadGlobal()
    }

    /** individual > helmet > global effective limits for the current active context. */
    val resolvedLimits: NPLimitsSet
        get() = resolveLimits(
            global = globalLimits,
            helmet = activeHelmetSerial?.let { helmetLimits[it] },
            individual = activeProfileId?.let { individualLimits[it] },
        )

    fun makeValidator(): NPProtocolValidator = NPProtocolValidator(resolvedLimits)

    // MARK: Mutations

    fun saveGlobalLimits(limits: NPLimitsSet) {
        globalLimits = limits.copy(level = NPLimitsSet.LimitLevel.GLOBAL)
        persistGlobal()
    }

    fun clearGlobalLimits() {
        globalLimits = null
        kv.remove(GLOBAL_KEY)
    }

    fun saveHelmetLimits(limits: NPLimitsSet, serial: String) {
        helmetLimits = helmetLimits + (serial to limits.copy(level = NPLimitsSet.LimitLevel.HELMET, helmetId = serial))
    }

    fun saveIndividualLimits(limits: NPLimitsSet, profileId: String) {
        individualLimits = individualLimits + (profileId to limits.copy(level = NPLimitsSet.LimitLevel.INDIVIDUAL))
    }

    fun saveProfile(profile: NPIndividualProfile) {
        profiles = if (profiles.any { it.id == profile.id }) {
            profiles.map { if (it.id == profile.id) profile else it }
        } else {
            profiles + profile
        }
    }

    fun deleteProfile(id: String) {
        profiles = profiles.filterNot { it.id == id }
        individualLimits = individualLimits - id
        if (activeProfileId == id) activeProfileId = null
    }

    // MARK: NPPS import / export

    fun exportLimitsAsNPPS(limits: NPLimitsSet): String = NPPSSerializer().serializeLimits(limits)

    fun importLimitsFromNPPS(text: String): NPLimitsSet {
        val entries = NPPSParser(NPPSLexer(text).tokenize()).parse()
        return entries.filterIsInstance<NPProtocolEntry.Limits>().firstOrNull()?.limits
            ?: throw NPPSError("No limits block found in script", 0)
    }

    // MARK: Persistence (global tier, NPPS text)

    private fun loadGlobal() {
        val text = kv.getString(GLOBAL_KEY) ?: return
        globalLimits = runCatching { importLimitsFromNPPS(text) }.getOrNull()
    }

    private fun persistGlobal() {
        globalLimits?.let { kv.putString(GLOBAL_KEY, exportLimitsAsNPPS(it)) } ?: kv.remove(GLOBAL_KEY)
    }
}
