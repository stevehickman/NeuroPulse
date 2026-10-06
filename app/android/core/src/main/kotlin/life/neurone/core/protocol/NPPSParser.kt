package life.neurone.core.protocol

import life.neurone.core.npps.NppsCore
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

// =============================================================================
// NPPS parsing on Android: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through JNI.
//
// This used to be a 1,265-line hand-written lexer and parser, one of five that drifted. There is one
// parser now, and it is in Rust: what an `.npps` file means, and what refuses it, is decided there and
// is the same on every runtime. What is left here is not language: building the Kotlin models from the
// core's JSON, which means supplying what the core leaves out because it is not the same twice (a random
// id for a protocol that states none) and the Kotlin spelling of each field.
// =============================================================================

/** A refusal: the core's message, `Line N: …` when it names a line. */
class NPPSError(val messageText: String, val line: Int) :
    Exception("Line $line: $messageText")

/**
 * Parse NPPS text. Protocols and composites come back in file order, then the file's zones, conditions
 * and limits sets in file order (the core reports each kind in its own list, so their order across
 * kinds is not kept). Wavelength rules are parsed and validated by the core and not consumed here:
 * this runtime compiles against the shipped default rules only.
 */
object NPPSParser {

    fun parse(text: String): List<NPProtocolEntry> {
        val parsed = try {
            NppsCore.parse(text)
        } catch (e: IllegalArgumentException) {
            throw refusal(e)
        }
        return entriesOf(parsed)
    }

    private val LINE_PREFIX = Regex("^Line (\\d+): ([\\s\\S]*)$")

    internal fun refusal(e: IllegalArgumentException): NPPSError {
        val message = e.message.orEmpty()
        val m = LINE_PREFIX.matchEntire(message)
        return if (m != null) NPPSError(m.groupValues[2], m.groupValues[1].toInt()) else NPPSError(message, 0)
    }

    private fun entriesOf(parsed: JsonObject): List<NPProtocolEntry> {
        val out = ArrayList<NPProtocolEntry>()
        for (e in parsed["entries"]!!.jsonArray) {
            val o = e.jsonObject
            when (o["kind"]!!.jsonPrimitive.content) {
                "single" -> out.add(NPProtocolEntry.Single(protocolOf(o["protocol"]!!.jsonObject)))
                "composite" -> out.add(NPProtocolEntry.Composite(compositeOf(o["composite"]!!.jsonObject)))
            }
        }
        for (z in parsed["zones"]!!.jsonArray) out.add(NPProtocolEntry.Zone(zoneOf(z.jsonObject)))
        for (c in parsed["conditions"]!!.jsonArray) out.add(NPProtocolEntry.Condition(conditionOf(c.jsonObject)))
        for (l in parsed["limits"]!!.jsonArray) out.add(NPProtocolEntry.Limits(limitsOf(l.jsonObject)))
        return out
    }

    // ── field readers ────────────────────────────────────────────────────────

    private fun JsonObject.str(k: String, d: String = ""): String = (this[k] as? JsonPrimitive)?.contentOrNull ?: d
    private fun JsonObject.optStr(k: String): String? = (this[k] as? JsonPrimitive)?.takeIf { it !is JsonNull }?.contentOrNull
    private fun JsonObject.dbl(k: String, d: Double): Double = (this[k] as? JsonPrimitive)?.doubleOrNull ?: d
    private fun JsonObject.optDbl(k: String): Double? = (this[k] as? JsonPrimitive)?.doubleOrNull
    private fun JsonObject.int(k: String, d: Int): Int = (this[k] as? JsonPrimitive)?.doubleOrNull?.toInt() ?: d
    private fun JsonObject.optInt(k: String): Int? = (this[k] as? JsonPrimitive)?.doubleOrNull?.toInt()
    private fun JsonObject.bool(k: String, d: Boolean): Boolean = (this[k] as? JsonPrimitive)?.booleanOrNull ?: d
    private fun JsonObject.optBool(k: String): Boolean? = (this[k] as? JsonPrimitive)?.booleanOrNull
    private fun JsonObject.strList(k: String): List<String>? =
        (this[k] as? JsonArray)?.map { it.jsonPrimitive.content }
    private fun JsonObject.sub(k: String): JsonObject? = this[k] as? JsonObject

    // ── protocols and composites ─────────────────────────────────────────────

    private fun uuidOrRandom(id: String?): java.util.UUID =
        id?.let { parseUuidOrNull(it) } ?: java.util.UUID.randomUUID()

    private fun referencesOf(o: JsonObject): List<NPProtocolReference> =
        (o["references"] as? JsonArray)?.map { r ->
            if (r is JsonPrimitive) NPProtocolReference(url = r.content)
            else NPProtocolReference(url = r.jsonObject.str("url"), label = r.jsonObject.optStr("label"))
        } ?: emptyList()

    private fun protocolOf(p: JsonObject): NPProtocolDefinition {
        val timing = p["timingMode"]!!.jsonObject
        return NPProtocolDefinition(
            id = uuidOrRandom(p.optStr("id")),
            name = p.str("name"),
            description = p.str("description"),
            author = p.str("author", "NeurOne"),
            version = p.str("version", "1.0"),
            tags = p.strList("tags") ?: emptyList(),
            isPredefined = p.bool("isPredefined", false),
            isReadOnly = p.bool("isReadOnly", false),
            timingMode = if (timing.str("type") == "interval_count") NPTimingMode.IntervalCount(timing.int("count", 1))
            else NPTimingMode.Duration(timing.int("seconds", 20 * 60)),
            modalities = p["modalities"]!!.jsonArray.map { modalityOf(it.jsonObject) },
            conditions = p.strList("conditions") ?: emptyList(),
            references = referencesOf(p),
        )
    }

    private fun compositeOf(c: JsonObject): NPCompositeProtocol = NPCompositeProtocol(
        id = uuidOrRandom(c.optStr("id")),
        name = c.str("name"),
        description = c.str("description"),
        author = c.str("author", "NeurOne"),
        version = c.str("version", "1.0"),
        tags = c.strList("tags") ?: emptyList(),
        isPredefined = c.bool("isPredefined", false),
        isReadOnly = c.bool("isReadOnly", false),
        layers = c["layers"]!!.jsonArray.map { l ->
            val o = l.jsonObject
            NPCompositeLayer(
                protocolName = o.str("protocolName"),
                startOffsetSeconds = o.int("startOffsetSeconds", 0),
                durationSeconds = o.optInt("durationSeconds"),
                intensityScale = o.dbl("intensityScale", 1.0),
            )
        },
        conflictResolution = NPCompositeProtocol.ConflictResolution.entries
            .firstOrNull { it.rawValue == c.str("conflictResolution") } ?: NPCompositeProtocol.ConflictResolution.MERGE,
        conditions = c.strList("conditions") ?: emptyList(),
        references = referencesOf(c),
    )

    private fun modalityOf(m: JsonObject): NPProtocolModality {
        val i = m["interval"]!!.jsonObject
        return NPProtocolModality(
            params = paramsOf(m.str("type"), m["params"]!!.jsonObject),
            interval = NPIntervalConfig(
                intervalOnSeconds = i.int("intervalOnSeconds", 0),
                intervalOffSeconds = i.int("intervalOffSeconds", 0),
                repeatCount = i.optInt("repeatCount"),
                startOffsetSeconds = i.int("startOffsetSeconds", 0),
            ),
            enabled = m.bool("enabled", true),
        )
    }

    private inline fun <reified E : Enum<E>> byName(raw: String?, default: E): E =
        enumValues<E>().firstOrNull { it.name.equals(raw, ignoreCase = true) } ?: default

    private fun paramsOf(type: String, p: JsonObject): NPModalityParams = when (type) {
        "pbm_transcranial" -> NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
            target = if (p.str("zones") == "clinician_selected") NPPBMTarget.ClinicianSelected
            else NPPBMTarget.Named(p.strList("zoneRefs") ?: listOf("All")),
            wavelength = NPPBMTranscranialParams.Wavelength(p.str("wavelength", "808nm")),
            irradianceMWcm2 = p.dbl("irradianceMWcm2", 300.0),
            frequencyHz = p.dbl("frequencyHz", 40.0),
            dutyCyclePercent = p.int("dutyCyclePercent", 25),
        ))
        "pbm_intranasal" -> NPModalityParams.PbmIntranasal(NPPBMIntranasalParams(
            wavelength = NPPBMTranscranialParams.Wavelength(p.str("wavelength", "660nm")),
            irradianceMWcm2 = p.dbl("irradianceMWcm2", 60.0),
            frequencyHz = p.dbl("frequencyHz", 10.0),
            dutyCyclePercent = p.int("dutyCyclePercent", 50),
        ))
        "eeg_neurofeedback" -> NPModalityParams.EegNeurofeedback(NPEEGNeurofeedbackParams(
            channels = byName(p.optStr("channels"), NPEEGNeurofeedbackParams.ChannelSelection.ALL),
            customChannels = p.strList("customChannels"),
            band = byName(p.optStr("band"), NPEEGNeurofeedbackParams.EEGBand.ALPHA),
            closedLoopEnabled = p.bool("closedLoopEnabled", true),
        ))
        "bes_tacs" -> NPModalityParams.BesTacs(NPBESTacsParams(
            frequencyHz = p.dbl("frequencyHz", 40.0),
            intensityMilliamps = p.dbl("intensityMilliamps", 0.5),
            waveform = byName(p.optStr("waveform"), NPBESTacsParams.Waveform.SINUSOIDAL),
        ))
        "tdcs" -> NPModalityParams.Tdcs(NPTDCSParams(
            intensityMilliamps = p.dbl("intensityMilliamps", 1.0),
            electrodePairs = (p["electrodePairs"] as? JsonArray)?.map { pair -> pair.jsonArray.map { it.jsonPrimitive.content } }
                ?: listOf(listOf("Fp1", "Fp2")),
            rampSeconds = p.int("rampSeconds", 30),
            electrodeAreaCm2 = p.dbl("electrodeAreaCm2", NPHardwareLimits.TDCS_DEFAULT_ELECTRODE_AREA_CM2),
        ))
        "vns_hrv" -> NPModalityParams.VnsHRV(NPVNSHRVParams(
            frequencyHz = p.dbl("frequencyHz", 25.0),
            intensityMilliamps = p.dbl("intensityMilliamps", 0.5),
            hrvProtocol = byName(p.optStr("hrvProtocol"), NPVNSHRVParams.HRVProtocol.STANDALONE),
            resonanceBreathingRate = p.dbl("resonanceBreathingRate", 6.0),
        ))
        "audio_entrainment" -> NPModalityParams.AudioEntrainment(NPAudioEntrainmentParams(
            binauralBeatsHz = p.optDbl("binauralBeatsHz"),
            isochronicTonesHz = p.optDbl("isochronicTonesHz"),
            noiseType = p.optStr("noiseType")?.let { n -> NPAudioEntrainmentParams.NoiseType.entries.firstOrNull { it.rawValue == n } },
            carrierHz = p.dbl("carrierHz", 200.0),
            volumeDb = p.dbl("volumeDb", 75.0),
            eegAdaptive = p.bool("eegAdaptive", true),
            boneConductionPacer = p.bool("boneConductionPacer", false),
        ))
        "visual_stimulation" -> NPModalityParams.VisualStimulation(NPVisualStimParams(
            frequencyHz = p.dbl("frequencyHz", 40.0),
            mode = NPVisualStimParams.VisualMode.entries.firstOrNull { it.rawValue == p.optStr("mode") }
                ?: NPVisualStimParams.VisualMode.BINOCULAR,
            emdrCadenceHz = p.dbl("emdrCadenceHz", 1.0),
            enableModeF = p.bool("enableModeF", false),
        ))
        "qeeg_21ch" -> NPModalityParams.Qeeg21ch(NPqEEG21chParams(
            montage = NPqEEG21chParams.Montage.entries.firstOrNull { it.rawValue == p.optStr("montage") }
                ?: NPqEEG21chParams.Montage.STANDARD_1020,
            sloretaEnabled = p.bool("sloretaEnabled", true),
            reference = NPqEEG21chParams.Reference.entries.firstOrNull { it.rawValue == p.optStr("reference") }
                ?: NPqEEG21chParams.Reference.LINKED_EAR,
        ))
        "tms" -> NPModalityParams.Tms(NPTMSParams(
            tmsProtocol = NPTMSParams.TMSProtocol.entries.firstOrNull { it.rawValue == p.optStr("tmsProtocol") }
                ?: NPTMSParams.TMSProtocol.RTMS,
            frequencyHz = p.dbl("frequencyHz", 10.0),
            intensityPercentMT = p.int("intensityPercentMT", 110),
            target = NPTMSParams.TMSTarget.fromRawValue(p.str("target")) ?: NPTMSParams.TMSTarget.DLPFC_L,
            pulseCount = p.int("pulseCount", 3000),
        ))
        "pbm_deep_1170nm" -> NPModalityParams.PbmDeep1170nm(NPDeepPBM1170Params(
            intensityMWcm2 = p.dbl("intensityMWcm2", 500.0),
            frequencyHz = p.dbl("frequencyHz", 10.0),
            dutyCyclePercent = p.int("dutyCyclePercent", 50),
        ))
        "clinical_tacs" -> NPModalityParams.ClinicalTacs(NPClinicalTacsParams(
            frequencyHz = p.dbl("frequencyHz", 40.0),
            intensityMilliamps = p.dbl("intensityMilliamps", 2.0),
            channelCount = p.int("channelCount", 8),
            waveform = byName(p.optStr("waveform"), NPBESTacsParams.Waveform.SINUSOIDAL),
        ))
        "hd_tdcs" -> NPModalityParams.HdTdcs(NPHDTdcsParams(
            target = NPTMSParams.TMSTarget.fromRawValue(p.str("target")) ?: NPTMSParams.TMSTarget.DLPFC_L,
            montage = NPHDTdcsParams.Montage.entries.firstOrNull { it.rawValue == p.optStr("montage") }
                ?: NPHDTdcsParams.Montage.RING_4X1,
            intensityMilliamps = p.dbl("intensityMilliamps", 1.5),
        ))
        "cervical_vns" -> NPModalityParams.CervicalVns(NPCervicalVnsParams(
            frequencyHz = p.dbl("frequencyHz", 25.0),
            intensityMilliamps = p.dbl("intensityMilliamps", 1.0),
        ))
        "vibrotactile_40hz" -> NPModalityParams.Vibrotactile40hz(NPVibrotactileParams(
            intensityG = p.dbl("intensityG", 0.9),
            syncToAudio = p.bool("syncToAudio", true),
            syncToVisual = p.bool("syncToVisual", true),
        ))
        else -> throw NPPSError("Unknown modality block type: '$type'", 0)
    }

    // ── zones, conditions, limits ────────────────────────────────────────────

    private fun zoneOf(z: JsonObject) = NPZoneDefinition(
        name = z.str("name"),
        sockets = (z["sockets"] as? JsonArray)?.map { it.jsonPrimitive.doubleOrNull!!.toInt() } ?: emptyList(),
        id = z.optStr("id"),
        description = z.optStr("description"),
        types = z.strList("types"),
        excludeTypes = z.bool("excludeTypes", false),
        isPredefined = z.bool("isPredefined", false),
    )

    private fun conditionOf(c: JsonObject) = NPConditionDefinition(
        name = c.str("name"),
        id = c.optStr("id"),
        link = c.str("link"),
        code = c.optStr("code"),
        description = c.optStr("description"),
    )

    private fun limitsOf(l: JsonObject): NPLimitsSet {
        val helmet = l.optStr("helmetId")
        val individual = l.optStr("individualId")
        val set = NPLimitsSet(
            id = java.util.UUID.randomUUID(),
            name = l.str("name"),
            description = l.str("description"),
            level = NPLimitsSet.LimitLevel.entries.firstOrNull { it.rawValue == l.str("level") }
                ?: NPLimitsSet.LimitLevel.GLOBAL,
            helmetId = helmet,
            individualId = individual?.let { parseUuidOrNull(it) },
        )
        l.sub("pbmTranscranial")?.let { set.pbmTranscranial = NPPBMTranscranialLimits(
            it.optDbl("maxIrradianceMWcm2"), it.optDbl("maxFrequencyHz"), it.optInt("maxDutyCyclePercent"),
            it.optDbl("maxSessionDoseJCm2"), it.optDbl("maxDailyDoseJCm2")) }
        l.sub("pbmIntranasal")?.let { set.pbmIntranasal = NPPBMIntranasalLimits(
            it.optDbl("maxIrradianceMWcm2"), it.optDbl("maxSessionDoseJCm2"), it.optInt("maxSessionDurationSeconds")) }
        l.sub("eegNeurofeedback")?.let { set.eegNeurofeedback = NPEEGNeurofeedbackLimits(
            it.strList("allowedBands"), it.optBool("requireClosedLoop")) }
        l.sub("besTacs")?.let { set.besTacs = NPBESTacsLimits(
            it.optDbl("maxIntensityMilliamps"), it.optDbl("maxFrequencyHz"), it.optDbl("minFrequencyHz"),
            it.optInt("maxSessionDurationSeconds"), it.optInt("maxSessionsPerDay")) }
        l.sub("tdcs")?.let { set.tdcs = NPTDCSLimits(
            it.optDbl("maxIntensityMilliamps"), it.optInt("maxSessionDurationSeconds"), it.optInt("maxSessionsPerDay")) }
        l.sub("vnsHrv")?.let { set.vnsHrv = NPVNSHRVLimits(
            it.optDbl("maxIntensityMilliamps"), it.optDbl("maxFrequencyHz"), it.optInt("maxSessionDurationSeconds"),
            it.strList("allowedProtocols")) }
        l.sub("audioEntrainment")?.let { set.audioEntrainment = NPAudioEntrainmentLimits(
            it.optDbl("maxVolumeDb"), it.optDbl("maxBinauralBeatsHz"), it.optDbl("maxIsochronicTonesHz")) }
        l.sub("visualStimulation")?.let { set.visualStimulation = NPVisualStimLimits(
            it.optDbl("maxFrequencyHz"), it.optDbl("minFrequencyHz"), it.strList("allowedModes"),
            it.optBool("blockHighRiskRange")) }
        l.sub("tms")?.let { set.tms = NPTMSLimits(
            it.optInt("maxIntensityPercentMT"), it.optInt("maxPulsesPerSession"), it.optInt("maxPulsesPerDay"),
            it.optInt("maxSessionsPerWeek"), it.strList("allowedProtocols"), it.strList("allowedTargets")) }
        l.sub("pbmDeep1170nm")?.let { set.pbmDeep1170nm = NPDeepPBMLimits(
            it.optDbl("maxIntensityMWcm2"), it.optInt("maxSessionDurationSeconds")) }
        l.sub("clinicalTacs")?.let { set.clinicalTacs = NPClinicalTacsLimits(
            it.optDbl("maxIntensityMilliamps"), it.optInt("maxSessionDurationSeconds")) }
        l.sub("hdTdcs")?.let { set.hdTdcs = NPHDTdcsLimits(
            it.optDbl("maxIntensityMilliamps"), it.optInt("maxSessionDurationSeconds"), it.strList("allowedMontages")) }
        l.sub("cervicalVns")?.let { set.cervicalVns = NPCervicalVnsLimits(
            it.optDbl("maxIntensityMilliamps"), it.optInt("maxSessionDurationSeconds")) }
        l.sub("vibrotactile40hz")?.let { set.vibrotactile40hz = NPVibrotactileLimits(
            it.optDbl("maxIntensityG"), it.optInt("maxSessionDurationSeconds")) }
        return set
    }
}
