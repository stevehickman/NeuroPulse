package life.neurone.core.npps

import life.neurone.core.protocol.NPAudioEntrainmentParams
import life.neurone.core.protocol.NPBESTacsParams
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
