package life.neurone.core.protocol

import java.util.UUID

// =============================================================================
// NPPS model types — Kotlin port of the model layer the NPPS engine operates on.
//
// PORT NOTE (Forge): the iOS engine (NPProtocolScripting.swift) references model
// types that live in NPProtocolDefinition.swift, NPModalityType.swift and
// NPDosageLimits.swift. On the Android side those files did not yet exist when
// this port was written (the :core module was empty). To keep the engine files
// self-contained, compilable, and unit-testable in isolation, the subset of the
// model layer that the lexer / parser / serializer actually touch is defined
// here. Fields not read or written by the engine (e.g. the three-tier limit
// resolution + source-map machinery in NPDosageLimits.swift) are intentionally
// omitted — the engine never references them, so porting them would be dead code.
//
// Swift `Date` (createdAt/modifiedAt) is dropped: the engine writes the current
// instant on parse and never serializes those fields, so they carry no behavior.
// UUID maps to java.util.UUID; Swift `UUID(uuidString:)` (nil on failure) maps to
// parseUuidOrNull(...) below.
// =============================================================================

/** Swift `UUID(uuidString:)` returns nil on malformed input; Java throws. */
internal fun parseUuidOrNull(text: String): UUID? =
    try {
        UUID.fromString(text)
    } catch (_: IllegalArgumentException) {
        null
    }

// MARK: - Modality type ------------------------------------------------------

/** Port of NPModalityType (String-raw enum). Only rawValue is used by the engine. */
enum class NPModalityType(val rawValue: String) {
    PBM_TRANSCRANIAL("pbm_transcranial"),
    PBM_INTRANASAL("pbm_intranasal"),
    EEG_NEUROFEEDBACK("eeg_neurofeedback"),
    BES_TACS("bes_tacs"),
    TDCS("tdcs"),
    VNS_HRV("vns_hrv"),
    AUDIO_ENTRAINMENT("audio_entrainment"),
    VISUAL_STIMULATION("visual_stimulation"),
    QEEG_21CH("qeeg_21ch"),
    TMS("tms"),
    PBM_DEEP_1170NM("pbm_deep_1170nm"),
    CLINICAL_TACS("clinical_tacs"),
    HD_TDCS("hd_tdcs"),
    CERVICAL_VNS("cervical_vns"),
    VIBROTACTILE_40HZ("vibrotactile_40hz");
}

// MARK: - Interval config ----------------------------------------------------

/** Port of NPIntervalConfig. `repeatCount == null` = run until session end. */
data class NPIntervalConfig(
    val intervalOnSeconds: Int,
    val intervalOffSeconds: Int,
    val repeatCount: Int?,
) {
    val isContinuous: Boolean get() = intervalOnSeconds == 0

    companion object {
        val CONTINUOUS = NPIntervalConfig(0, 0, null)
    }
}

// MARK: - Per-modality parameter structs -------------------------------------

data class NPPBMTranscranialParams(
    /**
     * Defaults to the whole vault. A default that resolves is deliberate: the
     * old default was the five-slot ALL, which silently became "every module"
     * for any target the parser did not recognise.
     */
    var target: NPPBMTarget = NPPBMTarget.Named(listOf("All")),
    var wavelength: Wavelength = Wavelength.BASE_660_808NM,
    var intensityPercent: Double = 75.0,
    var frequencyHz: Double = 20.0,
    var dutyCyclePercent: Int = 25,
) {

    enum class Wavelength(val rawValue: String) {
        BASE_660_808NM("660_808nm"),
        SMART_1064NM("1064nm"),
        TRI_660_808_1064("660_808_1064nm");

        val requiresSmartModule: Boolean
            get() = this == SMART_1064NM || this == TRI_660_808_1064
    }
}

data class NPPBMIntranasalParams(
    var intensityPercent: Double = 60.0,
    var frequencyHz: Double = 40.0,
    var dutyCyclePercent: Int = 25,
)

data class NPEEGNeurofeedbackParams(
    var channels: ChannelSelection = ChannelSelection.ALL,
    var customChannels: List<String>? = null,
    var band: EEGBand = EEGBand.ALPHA,
    var closedLoopEnabled: Boolean = true,
) {
    enum class ChannelSelection { ALL, FRONT, CENTRAL, CUSTOM }

    enum class EEGBand(val rawValue: String) {
        DELTA("delta"),
        THETA("theta"),
        ALPHA("alpha"),
        BETA("beta"),
        GAMMA("gamma"),
        ALPHA_THETA("alphaTheta"),
        GAMMA_THETA("gammaTheta");
    }
}

data class NPBESTacsParams(
    var frequencyHz: Double = 20.0,
    var intensityMilliamps: Double = 0.8,
    var waveform: Waveform = Waveform.SINUSOIDAL,
) {
    enum class Waveform(val rawValue: String) {
        SINUSOIDAL("sinusoidal"),
        SQUARE("square"),
        TRIANGULAR("triangular");
    }
}

data class NPTDCSParams(
    var intensityMilliamps: Double = 1.0,
    var electrodePairs: List<List<String>> = listOf(listOf("Fp1", "P3")),
    var rampSeconds: Int = 30,
)

data class NPVNSHRVParams(
    var frequencyHz: Double = 25.0,
    var intensityMilliamps: Double = 1.5,
    var hrvProtocol: HRVProtocol = HRVProtocol.STANDALONE,
    var resonanceBreathingRate: Double = 6.0,
) {
    enum class HRVProtocol(val rawValue: String) {
        STANDALONE("standalone"),
        TAVNS_SYNC("tavnsSync"),
        EEG_BIOFEEDBACK("eegBiofeedback"),
        COMBINED_PBM("combinedPBM");
    }
}

data class NPAudioEntrainmentParams(
    var binauralBeatsHz: Double? = 20.0,
    var isochronicTonesHz: Double? = null,
    var noiseType: NoiseType? = NoiseType.PINK,
    var carrierHz: Double = 440.0,
    var volumePercent: Double = 60.0,
    var eegAdaptive: Boolean = true,
    var boneConductionPacer: Boolean = true,
) {
    enum class NoiseType(val rawValue: String) {
        PINK("pink"),
        BROWN("brown");
    }
}

data class NPVisualStimParams(
    var frequencyHz: Double = 40.0,
    var mode: VisualMode = VisualMode.BINOCULAR,
    var emdrCadenceHz: Double = 1.0,
    var enableModeF: Boolean = false,
) {
    enum class VisualMode(val rawValue: String) {
        BINOCULAR("binocular"),
        EMDR("emdr"),
        RETINAL_PBM("retinalPBM"),
        MODE_F("modeF");
    }
}

data class NPqEEG21chParams(
    var montage: Montage = Montage.STANDARD_1020,
    var sloretaEnabled: Boolean = true,
    var reference: Reference = Reference.LINKED_EAR,
) {
    enum class Montage(val rawValue: String) {
        STANDARD_1020("standard_1020"),
        CUSTOM("custom");
    }

    enum class Reference(val rawValue: String) {
        LINKED_EAR("linked_ear"),
        CZ("cz"),
        AVERAGE("average");
    }
}

data class NPTMSParams(
    var tmsProtocol: TMSProtocol = TMSProtocol.RTMS,
    var frequencyHz: Double = 10.0,
    var intensityPercentMT: Int = 80,
    var target: TMSTarget = TMSTarget.DLPFC_L,
    var pulseCount: Int = 1200,
) {
    enum class TMSProtocol(val rawValue: String) {
        RTMS("rTMS"),
        TBS("TBS"),
        ITBS("iTBS");
    }

    enum class TMSTarget(val rawValue: String) {
        DLPFC_L("DLPFC_L"),
        DLPFC_R("DLPFC_R"),
        VLPFC_L("VLPFC_L"),
        ACC("ACC"),
        MPFC("MPFC"),
        M1_L("M1_L"),
        M1_R("M1_R");

        companion object {
            /** Port of Swift `TMSTarget(rawValue:)` — nil on no match. */
            fun fromRawValue(raw: String): TMSTarget? = entries.firstOrNull { it.rawValue == raw }
        }
    }
}

data class NPDeepPBM1170Params(
    var intensityMWcm2: Double = 500.0,
    var frequencyHz: Double = 40.0,
    var dutyCyclePercent: Int = 25,
)

data class NPClinicalTacsParams(
    var frequencyHz: Double = 40.0,
    var intensityMilliamps: Double = 2.0,
    var channelCount: Int = 8,
    var waveform: NPBESTacsParams.Waveform = NPBESTacsParams.Waveform.SINUSOIDAL,
)

data class NPHDTdcsParams(
    var target: NPTMSParams.TMSTarget = NPTMSParams.TMSTarget.DLPFC_L,
    var montage: Montage = Montage.RING_4X1,
    var intensityMilliamps: Double = 1.5,
) {
    enum class Montage(val rawValue: String) {
        RING_4X1("ring_4x1"),
        BILATERAL_4X1("bilateral_4x1"),
        STANDARD_2EL("standard_2_electrode");
    }
}

data class NPCervicalVnsParams(
    var frequencyHz: Double = 25.0,
    var intensityMilliamps: Double = 1.5,
)

data class NPVibrotactileParams(
    var frequencyHz: Double = 40.0,
    var intensityG: Double = 0.9,
    var syncToAudio: Boolean = true,
    var syncToVisual: Boolean = true,
)

// MARK: - NPModalityParams (discriminated union) -----------------------------

/**
 * Port of the Swift `NPModalityParams` enum with associated values. Each case
 * becomes a `data class` carrying its params struct; `modalityType` mirrors the
 * Swift computed property the serializer relies on.
 */
sealed class NPModalityParams {
    abstract val modalityType: NPModalityType

    data class PbmTranscranial(val params: NPPBMTranscranialParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.PBM_TRANSCRANIAL
    }

    data class PbmIntranasal(val params: NPPBMIntranasalParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.PBM_INTRANASAL
    }

    data class EegNeurofeedback(val params: NPEEGNeurofeedbackParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.EEG_NEUROFEEDBACK
    }

    data class BesTacs(val params: NPBESTacsParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.BES_TACS
    }

    data class Tdcs(val params: NPTDCSParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.TDCS
    }

    data class VnsHRV(val params: NPVNSHRVParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.VNS_HRV
    }

    data class AudioEntrainment(val params: NPAudioEntrainmentParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.AUDIO_ENTRAINMENT
    }

    data class VisualStimulation(val params: NPVisualStimParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.VISUAL_STIMULATION
    }

    data class Qeeg21ch(val params: NPqEEG21chParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.QEEG_21CH
    }

    data class Tms(val params: NPTMSParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.TMS
    }

    data class PbmDeep1170nm(val params: NPDeepPBM1170Params) : NPModalityParams() {
        override val modalityType get() = NPModalityType.PBM_DEEP_1170NM
    }

    data class ClinicalTacs(val params: NPClinicalTacsParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.CLINICAL_TACS
    }

    data class HdTdcs(val params: NPHDTdcsParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.HD_TDCS
    }

    data class CervicalVns(val params: NPCervicalVnsParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.CERVICAL_VNS
    }

    data class Vibrotactile40hz(val params: NPVibrotactileParams) : NPModalityParams() {
        override val modalityType get() = NPModalityType.VIBROTACTILE_40HZ
    }
}

// MARK: - NPProtocolModality -------------------------------------------------

data class NPProtocolModality(
    val params: NPModalityParams,
    val interval: NPIntervalConfig = NPIntervalConfig.CONTINUOUS,
    val enabled: Boolean = true,
    val id: UUID = UUID.randomUUID(),
) {
    val modalityType: NPModalityType get() = params.modalityType
}

// MARK: - Timing mode --------------------------------------------------------

/** Port of NPProtocolDefinition.TimingMode. */
sealed class NPTimingMode {
    data class Duration(val seconds: Int) : NPTimingMode()
    data class IntervalCount(val count: Int) : NPTimingMode()
}

// MARK: - NPProtocolDefinition -----------------------------------------------

data class NPProtocolDefinition(
    var id: UUID = UUID.randomUUID(),
    var name: String,
    var description: String = "",
    var author: String = "NeurOne",
    var version: String = "1.0",
    var tags: List<String> = emptyList(),
    var isPredefined: Boolean = false,
    var isReadOnly: Boolean = false,
    var timingMode: NPTimingMode = NPTimingMode.Duration(20 * 60),
    var modalities: List<NPProtocolModality> = emptyList(),
    /**
     * Clinical conditions this protocol targets (NP-NPPS-REF-001 §3). Each name
     * must resolve to a loaded `condition` block — see [validateNamespaceReferences].
     */
    var conditions: List<String> = emptyList(),
    /** Evidence and applicability links (§3): a bare URL, or a [label, url] pair. */
    var references: List<NPProtocolReference> = emptyList(),
)

/** A `references` entry: a bare URL/path, or a labelled link. */
data class NPProtocolReference(val url: String, val label: String? = null) {
    /** What a link should read as: the label when one was authored. */
    val displayText: String get() = label ?: url
}

// MARK: - Composite ----------------------------------------------------------

data class NPCompositeLayer(
    val protocolName: String,
    val startOffsetSeconds: Int = 0,
    val durationSeconds: Int? = null,
    val intensityScale: Double = 1.0,
    val id: UUID = UUID.randomUUID(),
)

data class NPCompositeProtocol(
    var id: UUID = UUID.randomUUID(),
    var name: String,
    var description: String = "",
    var author: String = "NeurOne",
    var version: String = "1.0",
    var tags: List<String> = emptyList(),
    var isPredefined: Boolean = false,
    var isReadOnly: Boolean = false,
    var layers: List<NPCompositeLayer> = emptyList(),
    var conflictResolution: ConflictResolution = ConflictResolution.MERGE,
    var conditions: List<String> = emptyList(),
    var references: List<NPProtocolReference> = emptyList(),
) {
    enum class ConflictResolution(val rawValue: String) {
        MERGE("merge"),
        SEQUENTIAL("sequential"),
        OVERRIDE("override");
    }
}

// MARK: - Dosage limits ------------------------------------------------------
// Only the fields the parser reads and the serializer writes are ported. All
// fields are nullable — null = "no limit configured at this tier".

data class NPPBMTranscranialLimits(
    var maxIntensityPercent: Double? = null,
    var maxFrequencyHz: Double? = null,
    var maxDutyCyclePercent: Int? = null,
    var maxSessionDoseJCm2: Double? = null,
    var maxDailyDoseJCm2: Double? = null,
)

data class NPPBMIntranasalLimits(
    var maxIntensityPercent: Double? = null,
    var maxSessionDoseJCm2: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
)

data class NPEEGNeurofeedbackLimits(
    var allowedBands: List<String>? = null,
    var requireClosedLoop: Boolean? = null,
)

data class NPBESTacsLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxFrequencyHz: Double? = null,
    var minFrequencyHz: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
    var maxSessionsPerDay: Int? = null,
)

data class NPTDCSLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
    var maxSessionsPerDay: Int? = null,
)

data class NPVNSHRVLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxFrequencyHz: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
    var allowedProtocols: List<String>? = null,
)

data class NPAudioEntrainmentLimits(
    var maxVolumePercent: Double? = null,
    var maxBinauralBeatsHz: Double? = null,
    var maxIsochronicTonesHz: Double? = null,
)

data class NPVisualStimLimits(
    var maxFrequencyHz: Double? = null,
    var minFrequencyHz: Double? = null,
    var allowedModes: List<String>? = null,
    var blockHighRiskRange: Boolean? = null,
)

data class NPTMSLimits(
    var maxIntensityPercentMT: Int? = null,
    var maxPulsesPerSession: Int? = null,
    var maxPulsesPerDay: Int? = null,
    var maxSessionsPerWeek: Int? = null,
    var allowedProtocols: List<String>? = null,
    var allowedTargets: List<String>? = null,
)

data class NPDeepPBMLimits(
    var maxIntensityMWcm2: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
)

data class NPClinicalTacsLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
)

data class NPHDTdcsLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
    var allowedMontages: List<String>? = null,
)

data class NPCervicalVnsLimits(
    var maxIntensityMilliamps: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
)

data class NPVibrotactileLimits(
    var maxIntensityG: Double? = null,
    var maxSessionDurationSeconds: Int? = null,
)

data class NPLimitsSet(
    var id: UUID = UUID.randomUUID(),
    var name: String,
    var description: String = "",
    var level: LimitLevel = LimitLevel.GLOBAL,
    var helmetId: String? = null,
    var individualId: UUID? = null,
    var pbmTranscranial: NPPBMTranscranialLimits? = null,
    var pbmIntranasal: NPPBMIntranasalLimits? = null,
    var eegNeurofeedback: NPEEGNeurofeedbackLimits? = null,
    var besTacs: NPBESTacsLimits? = null,
    var tdcs: NPTDCSLimits? = null,
    var vnsHrv: NPVNSHRVLimits? = null,
    var audioEntrainment: NPAudioEntrainmentLimits? = null,
    var visualStimulation: NPVisualStimLimits? = null,
    var tms: NPTMSLimits? = null,
    var pbmDeep1170nm: NPDeepPBMLimits? = null,
    var clinicalTacs: NPClinicalTacsLimits? = null,
    var hdTdcs: NPHDTdcsLimits? = null,
    var cervicalVns: NPCervicalVnsLimits? = null,
    var vibrotactile40hz: NPVibrotactileLimits? = null,
) {
    enum class LimitLevel(val rawValue: String) {
        GLOBAL("global"),
        HELMET("helmet"),
        INDIVIDUAL("individual");
    }
}

// MARK: - NPProtocolEntry ----------------------------------------------------

/** Port of the Swift `NPProtocolEntry` enum. */
sealed class NPProtocolEntry {
    data class Single(val protocol: NPProtocolDefinition) : NPProtocolEntry()
    data class Composite(val composite: NPCompositeProtocol) : NPProtocolEntry()
    data class Limits(val limits: NPLimitsSet) : NPProtocolEntry()

    /** A `zone` block: a named set of modules (NP-NPPS-REF-001 §8). */
    data class Zone(val zone: NPZoneDefinition) : NPProtocolEntry()

    /** A `condition` block: name -> external definition link (§9). */
    data class Condition(val condition: NPConditionDefinition) : NPProtocolEntry()
}

// ─── PBM transcranial target ──────────────────────────────────────────────────

/**
 * Where a PBM transcranial command lands on the helmet lattice. Port of the iOS
 * `NPPBMTarget` and the web `zones` + `zoneRefs` pair, expressed as a sum type
 * because the payload differs per case and "selector plus an optional list"
 * made invalid combinations representable.
 *
 * There are exactly two ways to name a target and no third. The five-slot
 * selectors (`all` / `front` / `rear` / `custom` + numeric indices) are gone
 * rather than retained-but-refused: there are no existing users, and a target
 * the parser does not understand must stop the file rather than become a second
 * way to say where light lands.
 */
sealed class NPPBMTarget {
    /** Named zones from 00-zones.npps. Zones overlap at the midline by design. */
    data class Named(val zoneNames: List<String>) : NPPBMTarget()

    /** Patient-specific: the operator chooses sockets before the protocol runs. */
    object ClinicianSelected : NPPBMTarget()

    /**
     * Resolve to 1-based socket ids, deduplicated and sorted. Two zones sharing
     * a midline socket must dose it once.
     *
     * Throws rather than falling back to any default, because a silently
     * substituted target is wrong-site stimulation.
     */
    fun resolveSockets(clinicianSockets: List<Int>? = null): List<Int> = when (this) {
        is Named -> {
            if (zoneNames.isEmpty()) throw NPPSError("zones: [] names no zone", 0)
            val out = sortedSetOf<Int>()
            for (name in zoneNames) {
                val ids = SocketZones.sockets(name)
                    ?: throw NPPSError("unknown zone '$name'", 0)
                out.addAll(ids)
            }
            if (out.isEmpty()) throw NPPSError("zones: ${zoneNames.joinToString(", ")} is empty", 0)
            out.toList()
        }
        is ClinicianSelected -> {
            val chosen = clinicianSockets?.takeIf { it.isNotEmpty() }
                ?: throw NPPSError("clinician_selected requires operator-chosen sockets", 0)
            chosen.toSortedSet().toList()
        }
    }

    /** Short human-readable form, for pickers and validation messages. */
    val displayName: String
        get() = when (this) {
            is Named -> if (zoneNames.isEmpty()) "No zones" else zoneNames.joinToString(" + ")
            is ClinicianSelected -> "Clinician-selected sockets"
        }
}

// ─── Zone definition (named set of modules) ───────────────────────────────────

/**
 * Element-type names mirror the firmware `np_elem_type_t` (np_module_map.h).
 * Unknown names are preserved verbatim rather than rejected: the element table
 * is hardware-revision data, and a zone naming a type this build has not heard
 * of is a forward-compatibility case, not a malformed file.
 */
typealias NPElementType = String

/**
 * A zone is a named SET OF MODULES, defined as an explicit list of socket
 * (major) addresses — the first half of the firmware two-level
 * (socket:element) scheme. Listing sockets directly makes arbitrary,
 * non-contiguous zones definable. An optional element-type filter restricts
 * which elements within those modules the zone selects.
 *
 * [sockets] is a SET: duplicates collapse and the list is sorted at parse time,
 * so unioning the two hemisphere zones of a lobe counts a shared midline socket
 * once. See NP-NPPS-REF-001 §8.
 */
data class NPZoneDefinition(
    val name: String,
    val sockets: List<Int>,
    val id: String? = null,
    val description: String? = null,
    val types: List<NPElementType>? = null,
    val excludeTypes: Boolean = false,
    /** Presence of an `id` marks the zone as shipped/read-only. */
    val isPredefined: Boolean = false,
)
