package life.neurone.core.npps

import life.neurone.core.protocol.NPAudioEntrainmentParams
import life.neurone.core.protocol.NPBESTacsParams
import life.neurone.core.protocol.NPCompositeProtocol
import life.neurone.core.protocol.NPConditionDefinition
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolReference
import life.neurone.core.protocol.NPZoneDefinition
import life.neurone.core.protocol.NPEEGNeurofeedbackParams
import life.neurone.core.protocol.NPIntervalConfig
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPPBMTarget
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.protocol.NPVNSHRVParams
import life.neurone.core.protocol.NPVisualStimParams
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray

// The Android models as the shared NPPS core reads them (common/npps/fields.json).
//
// The core takes the normalised protocol shape the web parser produces, so this is the one place
// Android's models are spelled in it. Two things are deliberate:
//   - Enum values are written the way the field table spells them, not the way these models'
//     `rawValue`s do: `hrvProtocol` is `tavns_sync` there and `tavnsSync` here. The models'
//     spellings are an Android-local habit, and the core compares strings.
//   - Nothing is validated, clamped or defaulted here. A value that is wrong reaches the core
//     as it is, and the core refuses it with the message every runtime gives.

private fun NPIntervalConfig.toJson(): JsonObject = buildJsonObject {
    put("intervalOnSeconds", intervalOnSeconds)
    put("intervalOffSeconds", intervalOffSeconds)
    repeatCount?.let { put("repeatCount", it) }
    if (startOffsetSeconds > 0) put("startOffsetSeconds", startOffsetSeconds)
}

private fun NPVNSHRVParams.HRVProtocol.wire(): String = when (this) {
    NPVNSHRVParams.HRVProtocol.STANDALONE -> "standalone"
    NPVNSHRVParams.HRVProtocol.TAVNS_SYNC -> "tavns_sync"
    NPVNSHRVParams.HRVProtocol.EEG_BIOFEEDBACK -> "eeg_biofeedback"
    NPVNSHRVParams.HRVProtocol.COMBINED_PBM -> "combined_pbm"
}

private fun NPEEGNeurofeedbackParams.ChannelSelection.wire(): String = when (this) {
    NPEEGNeurofeedbackParams.ChannelSelection.ALL -> "all"
    NPEEGNeurofeedbackParams.ChannelSelection.FRONT -> "front"
    NPEEGNeurofeedbackParams.ChannelSelection.CENTRAL -> "central"
    NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM -> "custom"
}

private fun NPEEGNeurofeedbackParams.EEGBand.wire(): String = when (this) {
    NPEEGNeurofeedbackParams.EEGBand.ALPHA_THETA -> "alpha_theta"
    NPEEGNeurofeedbackParams.EEGBand.GAMMA_THETA -> "gamma_theta"
    else -> rawValue
}

private fun NPModalityParams.toJsonParams(): Pair<String, JsonObject> = when (this) {
    is NPModalityParams.PbmTranscranial -> "pbm_transcranial" to buildJsonObject {
        when (val t = params.target) {
            is NPPBMTarget.Named -> {
                put("zones", "named")
                putJsonArray("zoneRefs") { t.zoneNames.forEach { add(JsonPrimitive(it)) } }
            }
            NPPBMTarget.ClinicianSelected -> put("zones", "clinician_selected")
        }
        put("wavelength", params.wavelength.rawValue)
        put("irradianceMWcm2", params.irradianceMWcm2)
        put("frequencyHz", params.frequencyHz)
        put("dutyCyclePercent", params.dutyCyclePercent)
    }
    is NPModalityParams.PbmIntranasal -> "pbm_intranasal" to buildJsonObject {
        put("wavelength", params.wavelength.rawValue)
        put("irradianceMWcm2", params.irradianceMWcm2)
        put("frequencyHz", params.frequencyHz)
        put("dutyCyclePercent", params.dutyCyclePercent)
    }
    is NPModalityParams.EegNeurofeedback -> "eeg_neurofeedback" to buildJsonObject {
        put("channels", params.channels.wire())
        params.customChannels?.let { c -> putJsonArray("customChannels") { c.forEach { add(JsonPrimitive(it)) } } }
        put("band", params.band.wire())
        put("closedLoopEnabled", params.closedLoopEnabled)
    }
    is NPModalityParams.BesTacs -> "bes_tacs" to buildJsonObject {
        put("frequencyHz", params.frequencyHz)
        put("intensityMilliamps", params.intensityMilliamps)
        put("waveform", params.waveform.rawValue)
    }
    is NPModalityParams.Tdcs -> "tdcs" to buildJsonObject {
        put("intensityMilliamps", params.intensityMilliamps)
        put("electrodePairs", buildJsonArray {
            params.electrodePairs.forEach { p -> add(buildJsonArray { p.forEach { add(JsonPrimitive(it)) } }) }
        })
        put("rampSeconds", params.rampSeconds)
        put("electrodeAreaCm2", params.electrodeAreaCm2)
    }
    is NPModalityParams.VnsHRV -> "vns_hrv" to buildJsonObject {
        put("frequencyHz", params.frequencyHz)
        put("intensityMilliamps", params.intensityMilliamps)
        put("hrvProtocol", params.hrvProtocol.wire())
        put("resonanceBreathingRate", params.resonanceBreathingRate)
    }
    is NPModalityParams.AudioEntrainment -> "audio_entrainment" to buildJsonObject {
        params.binauralBeatsHz?.let { put("binauralBeatsHz", it) }
        params.isochronicTonesHz?.let { put("isochronicTonesHz", it) }
        params.noiseType?.let { put("noiseType", it.rawValue) }
        put("carrierHz", params.carrierHz)
        put("volumeDb", params.volumeDb)
        put("eegAdaptive", params.eegAdaptive)
        put("boneConductionPacer", params.boneConductionPacer)
    }
    is NPModalityParams.VisualStimulation -> "visual_stimulation" to buildJsonObject {
        put("frequencyHz", params.frequencyHz)
        put("mode", params.mode.rawValue)
        put("emdrCadenceHz", params.emdrCadenceHz)
        put("enableModeF", params.enableModeF)
    }
    is NPModalityParams.Qeeg21ch -> "qeeg_21ch" to buildJsonObject {
        put("montage", params.montage.rawValue)
        put("sloretaEnabled", params.sloretaEnabled)
        put("reference", params.reference.rawValue)
    }
    is NPModalityParams.Tms -> "tms" to buildJsonObject {
        put("tmsProtocol", params.tmsProtocol.rawValue)
        put("frequencyHz", params.frequencyHz)
        put("intensityPercentMT", params.intensityPercentMT)
        put("target", params.target.rawValue)
        put("pulseCount", params.pulseCount)
    }
    is NPModalityParams.PbmDeep1170nm -> "pbm_deep_1170nm" to buildJsonObject {
        put("intensityMWcm2", params.intensityMWcm2)
        put("frequencyHz", params.frequencyHz)
        put("dutyCyclePercent", params.dutyCyclePercent)
    }
    is NPModalityParams.ClinicalTacs -> "clinical_tacs" to buildJsonObject {
        put("frequencyHz", params.frequencyHz)
        put("intensityMilliamps", params.intensityMilliamps)
        put("channelCount", params.channelCount)
        put("waveform", params.waveform.rawValue)
    }
    is NPModalityParams.HdTdcs -> "hd_tdcs" to buildJsonObject {
        put("target", params.target.rawValue)
        put("montage", params.montage.rawValue)
        put("intensityMilliamps", params.intensityMilliamps)
    }
    is NPModalityParams.CervicalVns -> "cervical_vns" to buildJsonObject {
        put("frequencyHz", params.frequencyHz)
        put("intensityMilliamps", params.intensityMilliamps)
    }
    is NPModalityParams.Vibrotactile40hz -> "vibrotactile_40hz" to buildJsonObject {
        // Not an `.npps` field (the firmware locks 40 Hz), but a model built in an editor can carry another, and the
        // validator warns about it. The compiler and serializer ignore it.
        put("frequencyHz", params.frequencyHz)
        put("intensityG", params.intensityG)
        put("syncToAudio", params.syncToAudio)
        put("syncToVisual", params.syncToVisual)
    }
}

/** The protocol in the shape [NppsCore.compile] takes: `timingMode` and `modalities`. */
internal fun NPProtocolDefinition.toNppsCoreJson(): JsonObject = buildJsonObject {
    put("timingMode", buildJsonObject {
        when (val t = timingMode) {
            is NPTimingMode.Duration -> { put("type", "duration"); put("seconds", t.seconds) }
            is NPTimingMode.IntervalCount -> { put("type", "interval_count"); put("count", t.count) }
        }
    })
    put("modalities", JsonArray(modalities.map { m ->
        val (type, params) = m.params.toJsonParams()
        buildJsonObject {
            put("type", type)
            put("enabled", m.enabled)
            put("params", params)
            put("interval", m.interval.toJson())
        }
    }))
}

/** Every zone name a PBM block of [this] targets by name. */
internal fun NPProtocolDefinition.namedZoneRefs(): Set<String> =
    modalities.mapNotNull { (it.params as? NPModalityParams.PbmTranscranial)?.params?.target as? NPPBMTarget.Named }
        .flatMap { it.zoneNames }.toSet()

// ── Whole entries, for the core's serializer and validator ───────────────────────────────────────────

private fun jsonStrings(items: List<String>): JsonArray = JsonArray(items.map { JsonPrimitive(it) })

private fun referencesJson(refs: List<NPProtocolReference>): JsonArray = JsonArray(refs.map { r ->
    if (r.label == null) JsonPrimitive(r.url) else buildJsonObject { put("label", r.label); put("url", r.url) }
})

/** The header fields a protocol and a composite share, as the core names them. */
private fun kotlinx.serialization.json.JsonObjectBuilder.header(
    id: life.neurone.core.common.UUID, name: String, description: String, author: String, version: String,
    tags: List<String>, isReadOnly: Boolean, conditions: List<String>, references: List<NPProtocolReference>,
) {
    put("id", id.toString())
    put("name", name)
    put("description", description)
    put("author", author)
    put("version", version)
    put("tags", jsonStrings(tags))
    if (isReadOnly) put("isReadOnly", true)
    if (conditions.isNotEmpty()) put("conditions", jsonStrings(conditions))
    if (references.isNotEmpty()) put("references", referencesJson(references))
}

private fun NPCompositeProtocol.toNppsCoreJson(): JsonObject = buildJsonObject {
    header(id, name, description, author, version, tags, isReadOnly, conditions, references)
    put("conflictResolution", conflictResolution.rawValue)
    put("layers", JsonArray(layers.map { l ->
        buildJsonObject {
            put("protocolName", l.protocolName)
            put("startOffsetSeconds", l.startOffsetSeconds)
            l.durationSeconds?.let { put("durationSeconds", it) }
            put("intensityScale", l.intensityScale)
        }
    }))
}

private fun NPZoneDefinition.toNppsCoreJson(): JsonObject = buildJsonObject {
    put("name", name)
    id?.let { put("id", it) }
    description?.let { put("description", it) }
    putJsonArray("sockets") { sockets.forEach { add(JsonPrimitive(it)) } }
    types?.let { put("types", jsonStrings(it)) }
    if (excludeTypes) put("excludeTypes", true)
}

private fun NPConditionDefinition.toNppsCoreJson(): JsonObject = buildJsonObject {
    put("name", name)
    id?.let { put("id", it) }
    put("link", link)
    code?.let { put("code", it) }
    description?.let { put("description", it) }
}

private fun NPLimitsSet.toNppsCoreJson(): JsonObject = buildJsonObject {
    put("name", name)
    put("description", description)
    put("level", level.rawValue)
    helmetId?.let { put("helmetId", it) }
    individualId?.let { put("individualId", it.toString()) }
    fun sub(key: String, vararg fields: Pair<String, Any?>) {
        val set = fields.filter { it.second != null }
        if (set.isEmpty()) return
        put(key, buildJsonObject {
            for ((k, v) in set) when (v) {
                is Double -> put(k, v)
                is Int -> put(k, v)
                is Boolean -> put(k, v)
                is List<*> -> put(k, jsonStrings(v.map { it.toString() }))
            }
        })
    }
    pbmTranscranial?.let { sub("pbmTranscranial", "maxIrradianceMWcm2" to it.maxIrradianceMWcm2, "maxFrequencyHz" to it.maxFrequencyHz,
        "maxDutyCyclePercent" to it.maxDutyCyclePercent, "maxSessionDoseJCm2" to it.maxSessionDoseJCm2, "maxDailyDoseJCm2" to it.maxDailyDoseJCm2) }
    pbmIntranasal?.let { sub("pbmIntranasal", "maxIrradianceMWcm2" to it.maxIrradianceMWcm2, "maxSessionDoseJCm2" to it.maxSessionDoseJCm2,
        "maxSessionDurationSeconds" to it.maxSessionDurationSeconds) }
    eegNeurofeedback?.let { sub("eegNeurofeedback", "allowedBands" to it.allowedBands, "requireClosedLoop" to it.requireClosedLoop) }
    besTacs?.let { sub("besTacs", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxFrequencyHz" to it.maxFrequencyHz,
        "minFrequencyHz" to it.minFrequencyHz, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds, "maxSessionsPerDay" to it.maxSessionsPerDay) }
    tdcs?.let { sub("tdcs", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds,
        "maxSessionsPerDay" to it.maxSessionsPerDay) }
    vnsHrv?.let { sub("vnsHrv", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxFrequencyHz" to it.maxFrequencyHz,
        "maxSessionDurationSeconds" to it.maxSessionDurationSeconds, "allowedProtocols" to it.allowedProtocols) }
    audioEntrainment?.let { sub("audioEntrainment", "maxVolumeDb" to it.maxVolumeDb, "maxBinauralBeatsHz" to it.maxBinauralBeatsHz,
        "maxIsochronicTonesHz" to it.maxIsochronicTonesHz) }
    visualStimulation?.let { sub("visualStimulation", "maxFrequencyHz" to it.maxFrequencyHz, "minFrequencyHz" to it.minFrequencyHz,
        "allowedModes" to it.allowedModes, "blockHighRiskRange" to it.blockHighRiskRange) }
    tms?.let { sub("tms", "maxIntensityPercentMT" to it.maxIntensityPercentMT, "maxPulsesPerSession" to it.maxPulsesPerSession,
        "maxPulsesPerDay" to it.maxPulsesPerDay, "maxSessionsPerWeek" to it.maxSessionsPerWeek,
        "allowedProtocols" to it.allowedProtocols, "allowedTargets" to it.allowedTargets) }
    pbmDeep1170nm?.let { sub("pbmDeep1170nm", "maxIntensityMWcm2" to it.maxIntensityMWcm2, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds) }
    clinicalTacs?.let { sub("clinicalTacs", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds) }
    hdTdcs?.let { sub("hdTdcs", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds,
        "allowedMontages" to it.allowedMontages) }
    cervicalVns?.let { sub("cervicalVns", "maxIntensityMilliamps" to it.maxIntensityMilliamps, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds) }
    vibrotactile40hz?.let { sub("vibrotactile40hz", "maxIntensityG" to it.maxIntensityG, "maxSessionDurationSeconds" to it.maxSessionDurationSeconds) }
}

/** The whole protocol, header and all, as the core's serializer and validator take it. */
internal fun NPProtocolDefinition.toNppsCoreFullJson(): JsonObject {
    val body = toNppsCoreJson()
    return buildJsonObject {
        header(id, name, description, author, version, tags, isReadOnly, conditions, references)
        for ((k, v) in body) put(k, v)
    }
}

/** An entry as one item of the core's serialize request, and (for a protocol or composite) its validate `entry`. */
internal fun NPProtocolEntry.toNppsCoreItem(): JsonObject = when (this) {
    is NPProtocolEntry.Single -> buildJsonObject { put("kind", "single"); put("protocol", protocol.toNppsCoreFullJson()) }
    is NPProtocolEntry.Composite -> buildJsonObject { put("kind", "composite"); put("composite", composite.toNppsCoreJson()) }
    is NPProtocolEntry.Zone -> buildJsonObject { put("kind", "zone"); put("zone", zone.toNppsCoreJson()) }
    is NPProtocolEntry.Condition -> buildJsonObject { put("kind", "condition"); put("condition", condition.toNppsCoreJson()) }
    is NPProtocolEntry.Limits -> buildJsonObject { put("kind", "limits"); put("limits", limits.toNppsCoreJson()) }
}

internal fun NPLimitsSet.toNppsCoreLimitsJson(): JsonObject = toNppsCoreJson()
