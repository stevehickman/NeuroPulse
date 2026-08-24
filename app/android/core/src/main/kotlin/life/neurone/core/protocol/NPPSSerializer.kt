package life.neurone.core.protocol

import kotlin.math.abs

// =============================================================================
// NPPS serializer + round-trip — Kotlin port of the Swift `NPPSSerializer` and
// the free `nppsRoundTrip(_:)` function.
//
// Locked bug fix preserved (project docs/status/completed-decisions.md):
//   (5) serializeTag() quotes any tag containing a character outside
//       [A-Za-z0-9_] and escapes embedded double quotes. It is used at every
//       tag-emission site so a serialize→reparse round-trip preserves
//       hyphenated tags (e.g. "wind-down") as a single tag rather than silently
//       splitting on the hyphen.
//
// Number formatting matches Swift string interpolation of Double/Int:
//   - Int(v)      → whole number, no decimal point.
//   - Double v    → Kotlin `v.toString()` yields "1.5" / "2.0", matching Swift.
// =============================================================================

class NPPSSerializer {

    fun serialize(entry: NPProtocolEntry): String = when (entry) {
        is NPProtocolEntry.Single -> serializeProtocol(entry.protocol)
        is NPProtocolEntry.Composite -> serializeComposite(entry.composite)
        is NPProtocolEntry.Limits -> serializeLimits(entry.limits)
        is NPProtocolEntry.Zone -> serializeZone(entry.zone)
        is NPProtocolEntry.Condition -> serializeCondition(entry.condition)
    }

    // MARK: Zone / Condition (NP-NPPS-REF-001 §8, §9) -------------------------

    private fun serializeZone(z: NPZoneDefinition): String {
        val lines = ArrayList<String>()
        lines.add("zone ${quote(z.name)} {")
        z.id?.let { lines.add("    id: ${quote(it)}") }
        z.description?.let { lines.add("    description: ${quote(it)}") }
        // Canonical membership on the way out as well as in: sorted and deduped,
        // so two equal zones serialize identically.
        lines.add("    sockets: [${z.sockets.distinct().sorted().joinToString(", ")}]")
        z.types?.takeIf { it.isNotEmpty() }?.let { lines.add("    types: [${it.joinToString(", ")}]") }
        if (z.excludeTypes) lines.add("    exclude_types: true")
        lines.add("}")
        return lines.joinToString("\n")
    }

    private fun serializeCondition(c: NPConditionDefinition): String {
        val lines = ArrayList<String>()
        lines.add("condition ${quote(c.name)} {")
        c.id?.let { lines.add("    id: ${quote(it)}") }
        lines.add("    link: ${quote(c.link)}")
        c.code?.let { lines.add("    code: ${quote(it)}") }
        c.description?.let { lines.add("    description: ${quote(it)}") }
        lines.add("}")
        return lines.joinToString("\n")
    }

    // MARK: Limits -----------------------------------------------------------

    fun serializeLimits(limits: NPLimitsSet): String {
        val lines = ArrayList<String>()
        lines.add("limits \"${limits.name}\" {")
        lines.add("    level: ${limits.level.rawValue}")
        limits.helmetId?.let { lines.add("    helmet_id: \"$it\"") }
        limits.individualId?.let { lines.add("    individual_id: \"$it\"") }
        if (limits.description.isNotEmpty()) lines.add("    description: \"${limits.description}\"")
        lines.add("")

        limits.pbmTranscranial?.let { lim ->
            lines.add("    pbm_transcranial {")
            lim.maxIntensityPercent?.let { lines.add("        max_intensity: ${it.toInt()}%") }
            lim.maxFrequencyHz?.let { lines.add("        max_frequency: ${formatHz(it)}") }
            lim.maxDutyCyclePercent?.let { lines.add("        max_duty_cycle: $it%") }
            lim.maxSessionDoseJCm2?.let { lines.add("        max_session_dose: ${formatDouble(it)}") }
            lim.maxDailyDoseJCm2?.let { lines.add("        max_daily_dose: ${formatDouble(it)}") }
            lines.add("    }")
        }
        limits.pbmIntranasal?.let { lim ->
            lines.add("    pbm_intranasal {")
            lim.maxIntensityPercent?.let { lines.add("        max_intensity: ${it.toInt()}%") }
            lim.maxSessionDoseJCm2?.let { lines.add("        max_session_dose: ${formatDouble(it)}") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lines.add("    }")
        }
        limits.eegNeurofeedback?.let { lim ->
            lines.add("    eeg_neurofeedback {")
            lim.allowedBands?.let { lines.add("        allowed_bands: [${it.joinToString(", ")}]") }
            lim.requireClosedLoop?.let { lines.add("        require_closed_loop: $it") }
            lines.add("    }")
        }
        limits.besTacs?.let { lim ->
            lines.add("    bes_tacs {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxFrequencyHz?.let { lines.add("        max_frequency: ${formatHz(it)}") }
            lim.minFrequencyHz?.let { lines.add("        min_frequency: ${formatHz(it)}") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lim.maxSessionsPerDay?.let { lines.add("        max_sessions_per_day: $it") }
            lines.add("    }")
        }
        limits.tdcs?.let { lim ->
            lines.add("    tdcs {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lim.maxSessionsPerDay?.let { lines.add("        max_sessions_per_day: $it") }
            lines.add("    }")
        }
        limits.vnsHrv?.let { lim ->
            lines.add("    vns_hrv {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxFrequencyHz?.let { lines.add("        max_frequency: ${formatHz(it)}") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lim.allowedProtocols?.let { lines.add("        allowed_protocols: [${it.joinToString(", ")}]") }
            lines.add("    }")
        }
        limits.audioEntrainment?.let { lim ->
            lines.add("    audio_entrainment {")
            lim.maxVolumePercent?.let { lines.add("        max_intensity: ${it.toInt()}%") }
            lim.maxBinauralBeatsHz?.let { lines.add("        max_binaural_beats: ${formatHz(it)}") }
            lim.maxIsochronicTonesHz?.let { lines.add("        max_isochronic_tones: ${formatHz(it)}") }
            lines.add("    }")
        }
        limits.visualStimulation?.let { lim ->
            lines.add("    visual_stimulation {")
            lim.maxFrequencyHz?.let { lines.add("        max_frequency: ${formatHz(it)}") }
            lim.minFrequencyHz?.let { lines.add("        min_frequency: ${formatHz(it)}") }
            lim.allowedModes?.let { lines.add("        allowed_modes: [${it.joinToString(", ")}]") }
            lim.blockHighRiskRange?.let { lines.add("        block_high_risk_range: $it") }
            lines.add("    }")
        }
        limits.tms?.let { lim ->
            lines.add("    tms {")
            lim.maxIntensityPercentMT?.let { lines.add("        max_intensity_pct_mt: $it") }
            lim.maxPulsesPerSession?.let { lines.add("        max_pulses_per_session: $it") }
            lim.maxPulsesPerDay?.let { lines.add("        max_pulses_per_day: $it") }
            lim.maxSessionsPerWeek?.let { lines.add("        max_sessions_per_week: $it") }
            lim.allowedProtocols?.let { lines.add("        allowed_protocols: [${it.joinToString(", ")}]") }
            lim.allowedTargets?.let { lines.add("        allowed_targets: [${it.joinToString(", ")}]") }
            lines.add("    }")
        }
        limits.pbmDeep1170nm?.let { lim ->
            lines.add("    pbm_deep_1170nm {")
            lim.maxIntensityMWcm2?.let { lines.add("        max_intensity: ${formatDouble(it)}mW_cm2") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lines.add("    }")
        }
        limits.clinicalTacs?.let { lim ->
            lines.add("    clinical_tacs {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lines.add("    }")
        }
        limits.hdTdcs?.let { lim ->
            lines.add("    hd_tdcs {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lim.allowedMontages?.let { lines.add("        allowed_montages: [${it.joinToString(", ")}]") }
            lines.add("    }")
        }
        limits.cervicalVns?.let { lim ->
            lines.add("    cervical_vns {")
            lim.maxIntensityMilliamps?.let { lines.add("        max_intensity: ${formatDouble(it)}mA") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lines.add("    }")
        }
        limits.vibrotactile40hz?.let { lim ->
            lines.add("    vibrotactile_40hz {")
            lim.maxIntensityG?.let { lines.add("        max_intensity: ${formatDouble(it)}G") }
            lim.maxSessionDurationSeconds?.let { lines.add("        max_session_duration: ${formatTime(it)}") }
            lines.add("    }")
        }

        lines.add("}")
        return lines.joinToString("\n")
    }

    // MARK: Protocol ---------------------------------------------------------

    private fun serializeProtocol(proto: NPProtocolDefinition): String {
        val lines = ArrayList<String>()
        lines.add("protocol \"${proto.name}\" {")
        lines.add("    id: \"${proto.id}\"")
        if (proto.description.isNotEmpty()) lines.add("    description: \"${proto.description}\"")
        if (proto.author != "NeurOne") lines.add("    author: \"${proto.author}\"")
        lines.add("    version: \"${proto.version}\"")
        if (proto.isReadOnly) lines.add("    readonly: true")
        if (proto.tags.isNotEmpty()) {
            lines.add("    tags: [${proto.tags.joinToString(", ") { serializeTag(it) }}]")
        }
        if (proto.conditions.isNotEmpty()) {
            lines.add("    conditions: [${proto.conditions.joinToString(", ") { quote(it) }}]")
        }
        if (proto.references.isNotEmpty()) {
            lines.add("    references: [${proto.references.joinToString(", ") { serializeReference(it) }}]")
        }
        when (val tm = proto.timingMode) {
            is NPTimingMode.Duration -> lines.add("    duration: ${formatTime(tm.seconds)}")
            is NPTimingMode.IntervalCount -> lines.add("    interval_count: ${tm.count}")
        }
        lines.add("")
        for (mod in proto.modalities) {
            if (!mod.enabled) lines.add("    # (disabled)")
            serializeModality(mod).forEach { lines.add("    $it") }
            lines.add("")
        }
        lines.add("}")
        return lines.joinToString("\n")
    }

    private fun serializeModality(mod: NPProtocolModality): List<String> {
        val lines = ArrayList<String>()
        lines.add("${mod.modalityType.rawValue} {")
        serializeParams(mod.params).forEach { lines.add("    $it") }
        if (!mod.interval.isContinuous) {
            lines.add("    interval_on: ${formatTime(mod.interval.intervalOnSeconds)}")
            lines.add("    interval_off: ${formatTime(mod.interval.intervalOffSeconds)}")
            val rc = mod.interval.repeatCount
            if (rc != null) {
                lines.add("    repeat: $rc")
            } else {
                lines.add("    repeat: until_end")
            }
        }
        lines.add("}")
        return lines
    }

    private fun serializeParams(params: NPModalityParams): List<String> = when (params) {
        is NPModalityParams.PbmTranscranial -> {
            val p = params.params
            val lines = ArrayList<String>()
            lines.add("intensity: ${p.intensityPercent.toInt()}%")
            lines.add("frequency: ${formatHz(p.frequencyHz)}")
            if (p.frequencyHz > 0) lines.add("duty_cycle: ${p.dutyCyclePercent}%")
            when (val t = p.target) {
                is NPPBMTarget.Named ->
                    lines.add("zones: [${t.zoneNames.joinToString(", ") { quote(it) }}]")
                is NPPBMTarget.ClinicianSelected -> lines.add("zones: clinician_selected")
            }
            // Quoted: a wavelength is digit-leading, so the bare form has not been
            // a legal value since NP-NPPS-REF-001 Rev 6. Emitting it unquoted meant
            // this serializer wrote NPPS text its own parser cannot read back, which
            // is what broke saving and reloading a user protocol.
            lines.add("wavelength: ${quote(p.wavelength.rawValue)}")
            lines
        }

        is NPModalityParams.PbmIntranasal -> {
            val p = params.params
            listOf(
                "intensity: ${p.intensityPercent.toInt()}%",
                "frequency: ${formatHz(p.frequencyHz)}",
                "duty_cycle: ${p.dutyCyclePercent}%",
            )
        }

        is NPModalityParams.EegNeurofeedback -> {
            val p = params.params
            val lines = ArrayList<String>()
            lines.add("band: ${p.band.rawValue}")
            when (p.channels) {
                NPEEGNeurofeedbackParams.ChannelSelection.ALL -> lines.add("channels: all")
                NPEEGNeurofeedbackParams.ChannelSelection.FRONT -> lines.add("channels: front")
                NPEEGNeurofeedbackParams.ChannelSelection.CENTRAL -> lines.add("channels: central")
                NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM -> {
                    val chs = (p.customChannels ?: emptyList()).joinToString(", ") { "\"$it\"" }
                    lines.add("channels: [$chs]")
                }
            }
            lines.add("closed_loop: ${p.closedLoopEnabled}")
            lines
        }

        is NPModalityParams.BesTacs -> {
            val p = params.params
            listOf(
                "frequency: ${formatHz(p.frequencyHz)}",
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
                "waveform: ${p.waveform.rawValue}",
            )
        }

        is NPModalityParams.Tdcs -> {
            val p = params.params
            val pairs = p.electrodePairs.joinToString(", ") { pair ->
                "[${pair.joinToString(", ") { "\"$it\"" }}]"
            }
            listOf(
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
                "electrode_pairs: [$pairs]",
                "ramp: ${p.rampSeconds}s  # hardware-enforced",
            )
        }

        is NPModalityParams.VnsHRV -> {
            val p = params.params
            listOf(
                "frequency: ${formatHz(p.frequencyHz)}",
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
                "hrv_protocol: ${p.hrvProtocol.rawValue}",
                "breathing_rate: ${formatDouble(p.resonanceBreathingRate)}",
            )
        }

        is NPModalityParams.AudioEntrainment -> {
            val p = params.params
            val lines = ArrayList<String>()
            p.binauralBeatsHz?.let { lines.add("binaural_hz: ${formatHz(it)}") }
            p.isochronicTonesHz?.let { lines.add("isochronic_hz: ${formatHz(it)}") }
            val nt = p.noiseType
            if (nt != null) lines.add("noise: ${nt.rawValue}") else lines.add("noise: none")
            lines.add("carrier_hz: ${formatHz(p.carrierHz)}")
            lines.add("volume: ${p.volumePercent.toInt()}%")
            lines.add("eeg_adaptive: ${p.eegAdaptive}")
            lines.add("bone_conduction_pacer: ${p.boneConductionPacer}")
            lines
        }

        is NPModalityParams.VisualStimulation -> {
            val p = params.params
            val lines = ArrayList<String>()
            lines.add("frequency: ${formatHz(p.frequencyHz)}")
            lines.add("mode: ${p.mode.rawValue}")
            if (p.mode == NPVisualStimParams.VisualMode.EMDR) {
                lines.add("emdr_cadence: ${formatHz(p.emdrCadenceHz)}")
            }
            if (p.enableModeF) lines.add("enable_mode_f: true")
            lines
        }

        is NPModalityParams.Qeeg21ch -> {
            val p = params.params
            listOf(
                "montage: ${p.montage.rawValue}",
                "sloreta_enabled: ${p.sloretaEnabled}",
                "reference: ${p.reference.rawValue}",
            )
        }

        is NPModalityParams.Tms -> {
            val p = params.params
            listOf(
                "protocol: ${p.tmsProtocol.rawValue}",
                "frequency: ${formatHz(p.frequencyHz)}",
                "intensity_percent_mt: ${p.intensityPercentMT}%",
                "target: ${p.target.rawValue}",
                "pulse_count: ${p.pulseCount}",
            )
        }

        is NPModalityParams.PbmDeep1170nm -> {
            val p = params.params
            listOf(
                "intensity: ${formatDouble(p.intensityMWcm2)}mW_cm2",
                "frequency: ${formatHz(p.frequencyHz)}",
                "duty_cycle: ${p.dutyCyclePercent}%",
            )
        }

        is NPModalityParams.ClinicalTacs -> {
            val p = params.params
            listOf(
                "frequency: ${formatHz(p.frequencyHz)}",
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
                "channel_count: ${p.channelCount}",
                "waveform: ${p.waveform.rawValue}",
            )
        }

        is NPModalityParams.HdTdcs -> {
            val p = params.params
            listOf(
                "target: ${p.target.rawValue}",
                "montage: ${p.montage.rawValue}",
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
            )
        }

        is NPModalityParams.CervicalVns -> {
            val p = params.params
            listOf(
                "frequency: ${formatHz(p.frequencyHz)}",
                "intensity: ${formatDouble(p.intensityMilliamps)}mA",
                "# cardiac interlock always enforced by safety MCU",
            )
        }

        is NPModalityParams.Vibrotactile40hz -> {
            val p = params.params
            listOf(
                "frequency: ${formatHz(p.frequencyHz)}  # locked at 40Hz",
                "intensity_g: ${formatDouble(p.intensityG)}G",
                "sync_to_audio: ${p.syncToAudio}",
                "sync_to_visual: ${p.syncToVisual}",
            )
        }
    }

    // MARK: Composite --------------------------------------------------------

    private fun serializeComposite(comp: NPCompositeProtocol): String {
        val lines = ArrayList<String>()
        lines.add("composite \"${comp.name}\" {")
        lines.add("    id: \"${comp.id}\"")
        if (comp.description.isNotEmpty()) lines.add("    description: \"${comp.description}\"")
        if (comp.author != "NeurOne") lines.add("    author: \"${comp.author}\"")
        lines.add("    version: \"${comp.version}\"")
        if (comp.isReadOnly) lines.add("    readonly: true")
        if (comp.tags.isNotEmpty()) {
            lines.add("    tags: [${comp.tags.joinToString(", ") { serializeTag(it) }}]")
        }
        if (comp.conditions.isNotEmpty()) {
            lines.add("    conditions: [${comp.conditions.joinToString(", ") { quote(it) }}]")
        }
        if (comp.references.isNotEmpty()) {
            lines.add("    references: [${comp.references.joinToString(", ") { serializeReference(it) }}]")
        }
        lines.add("    conflict_resolution: ${comp.conflictResolution.rawValue}")
        lines.add("")
        for (layer in comp.layers) {
            lines.add("    layer \"${layer.protocolName}\" {")
            lines.add("        start: ${formatTime(layer.startOffsetSeconds)}")
            layer.durationSeconds?.let { lines.add("        duration: ${formatTime(it)}") }
            if (abs(layer.intensityScale - 1.0) > 0.001) {
                lines.add("        intensity_scale: ${formatDouble(layer.intensityScale)}")
            }
            lines.add("    }")
            lines.add("")
        }
        lines.add("}")
        return lines.joinToString("\n")
    }

    // MARK: Formatting helpers ----------------------------------------------

    private fun formatTime(seconds: Int): String = when {
        seconds == 0 -> "0s"
        seconds % 3600 == 0 -> "${seconds / 3600}h"
        seconds % 60 == 0 -> "${seconds / 60}m"
        else -> "${seconds}s"
    }

    private fun formatHz(hz: Double): String = when {
        hz == 0.0 -> "0Hz  # CW"
        hz == hz.toInt().toDouble() -> "${hz.toInt()}Hz"
        else -> "${formatDouble(hz)}Hz"
    }

    /**
     * Mirrors Swift's `"\(aDouble)"` string interpolation: integral values print
     * without a fractional part ("2.0" style is NOT produced by Swift here for
     * whole doubles — Swift prints "2.0"). Swift actually prints "2.0" for a
     * Double whole number; Kotlin's `Double.toString()` does the same ("2.0"),
     * so a plain toString() is faithful. Kept as a single choke point so any
     * future locale/format concern is fixed in one place.
     */
    private fun formatDouble(value: Double): String = value.toString()

    /**
     * Bug fix (5): emit a tag as a bare ident when every character is a letter,
     * digit, or underscore; quote it otherwise (e.g. "wind-down") and escape any
     * embedded double quote so a serialize→reparse round-trip preserves the
     * original string without silently splitting on hyphens.
     */
    private fun serializeTag(tag: String): String {
        // A bare identifier may not start with a digit: a tag like "1064nm" is
        // all letters and digits but must still be quoted to re-parse, since
        // NP-NPPS-REF-001 Rev 6 removed digit-leading bare values.
        val isBareIdent = tag.isNotEmpty() &&
            (tag.first().isLetter() || tag.first() == '_') &&
            tag.all { it.isLetter() || it.isDigit() || it == '_' }
        return if (isBareIdent) tag else quote(tag)
    }

    /** A bare URL, or a `[label, url]` pair when the entry was labelled. */
    private fun serializeReference(r: NPProtocolReference): String =
        if (r.label == null) quote(r.url) else "[${quote(r.label)}, ${quote(r.url)}]"

    /** A quoted NPPS string with embedded quotes and backslashes escaped. */
    private fun quote(value: String): String =
        "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""
}

// MARK: - Round-trip ---------------------------------------------------------

/** Port of the free `nppsRoundTrip(_:)` function. */
fun nppsRoundTrip(text: String): String {
    val tokens = NPPSLexer(text).tokenize()
    val entries = NPPSParser(tokens).parse()
    val serializer = NPPSSerializer()
    return entries.joinToString("\n\n") { serializer.serialize(it) }
}
