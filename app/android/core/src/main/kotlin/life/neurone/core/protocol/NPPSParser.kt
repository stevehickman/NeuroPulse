package life.neurone.core.protocol

import java.util.UUID

// =============================================================================
// NPPS parser — Kotlin port of the Swift `NPPSParser`.
//
// Locked bug fix preserved (project docs/status/completed-decisions.md):
//   (4) FieldValue.asIdent also returns quoted-string cases, so
//       `wavelength: "660_808nm"` works in user-authored scripts.
//
// Unknown modality-limit sub-blocks are silently ignored (forward compat), and
// unknown modality blocks inside a `protocol` throw — matching the Swift source.
// =============================================================================

/**
 * Port of the private `NPPSFieldValue` enum. Public so tests / callers may use
 * the parser's parseFieldValue helpers, mirroring the Swift enum's visibility
 * within the parser module.
 */
sealed class NPPSFieldValue {
    data class Num(val value: Double) : NPPSFieldValue()
    data class NumberWithUnit(val value: Double, val unit: String) : NPPSFieldValue()
    data class Str(val value: String) : NPPSFieldValue()
    data class Ident(val value: String) : NPPSFieldValue()
    data class BoolVal(val value: Boolean) : NPPSFieldValue()
    data class Arr(val items: List<NPPSFieldValue>) : NPPSFieldValue()

    val asDouble: Double?
        get() = when (this) {
            is Num -> value
            is NumberWithUnit -> value
            else -> null
        }

    val asHz: Double?
        get() = when (this) {
            is NumberWithUnit -> if (unit == "Hz") value else null
            is Num -> value
            else -> null
        }

    val asMilliamps: Double?
        get() = when (this) {
            is NumberWithUnit -> if (unit == "mA") value else null
            is Num -> value
            else -> null
        }

    val asPercent: Double?
        get() = when (this) {
            is NumberWithUnit -> if (unit == "%") value else null
            is Num -> value
            else -> null
        }

    /** Time in whole seconds. m→×60, s→×1, h→×3600; bare number→seconds. */
    val asTime: Int?
        get() = when (this) {
            is NumberWithUnit -> when (unit) {
                "m" -> (value * 60).toInt()
                "s" -> value.toInt()
                "h" -> (value * 3600).toInt()
                else -> null
            }
            is Num -> value.toInt()
            else -> null
        }

    val asBool: Boolean?
        get() = if (this is BoolVal) value else null

    /** Bug fix (4): idents AND quoted strings both resolve as an ident value. */
    val asIdent: String?
        get() = when (this) {
            is Ident -> value
            is Str -> value
            else -> null
        }
}

class NPPSParser(private val tokens: List<NPPSLexeme>) {

    private var pos: Int = 0

    // MARK: Entry point ------------------------------------------------------

    fun parse(): List<NPProtocolEntry> {
        val entries = ArrayList<NPProtocolEntry>()
        while (currentToken() !is NPPSToken.Eof) {
            val cur = currentToken()
            if (cur is NPPSToken.Keyword) {
                when (cur.value) {
                    "protocol" -> entries.add(NPProtocolEntry.Single(parseProtocol()))
                    "composite" -> entries.add(NPProtocolEntry.Composite(parseComposite()))
                    "limits" -> entries.add(NPProtocolEntry.Limits(parseLimitsBlock()))
                    else -> throw NPPSError("Unexpected keyword: ${cur.value}", currentLine())
                }
            } else {
                throw NPPSError("Expected 'protocol', 'composite', or 'limits'", currentLine())
            }
        }
        return entries
    }

    // MARK: Limits block -----------------------------------------------------

    private fun parseLimitsBlock(): NPLimitsSet {
        val ln = currentLine()
        expectKeyword("limits")
        val name = parseString()
        expect(NPPSToken.LBrace)

        val limitsSet = NPLimitsSet(name = name, level = NPLimitsSet.LimitLevel.GLOBAL)

        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val keyTok = currentToken()
            if (keyTok !is NPPSToken.Ident) {
                throw NPPSError("Expected field name in limits block", currentLine())
            }
            val key = keyTok.value
            advance()

            when (currentToken()) {
                is NPPSToken.Colon -> {
                    advance()
                    when (key) {
                        "level" -> {
                            when (parseIdentOrString()) {
                                "global" -> limitsSet.level = NPLimitsSet.LimitLevel.GLOBAL
                                "helmet" -> limitsSet.level = NPLimitsSet.LimitLevel.HELMET
                                "individual" -> limitsSet.level = NPLimitsSet.LimitLevel.INDIVIDUAL
                                else -> { /* unknown level ignored, matching Swift */ }
                            }
                        }
                        "helmet_id" -> {
                            limitsSet.helmetId = parseString()
                            limitsSet.level = NPLimitsSet.LimitLevel.HELMET
                        }
                        "individual_id" -> {
                            val uuidStr = parseString()
                            limitsSet.individualId = parseUuidOrNull(uuidStr)
                            limitsSet.level = NPLimitsSet.LimitLevel.INDIVIDUAL
                        }
                        "description" -> limitsSet.description = parseString()
                        else -> skipValue()
                    }
                }
                is NPPSToken.LBrace -> parseLimitsSubBlock(key, limitsSet)
                else -> throw NPPSError(
                    "Expected ':' or '{' after '$key' in limits block", currentLine()
                )
            }
        }

        expect(NPPSToken.RBrace)
        if (name.isEmpty()) throw NPPSError("Limits name cannot be empty", ln)
        return limitsSet
    }

    private fun parseLimitsSubBlock(key: String, limitsSet: NPLimitsSet) {
        expect(NPPSToken.LBrace)
        val fields = HashMap<String, NPPSFieldValue>()
        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val fk = currentToken()
            if (fk !is NPPSToken.Ident) {
                throw NPPSError("Expected field name in limits sub-block", currentLine())
            }
            advance()
            expect(NPPSToken.Colon)
            fields[fk.value] = parseFieldValue()
        }
        expect(NPPSToken.RBrace)

        // Helper to pull a string whitelist out of an array field.
        fun stringList(fv: NPPSFieldValue?): List<String>? {
            val arr = (fv as? NPPSFieldValue.Arr) ?: return null
            return arr.items.mapNotNull { it.asIdent }
        }

        when (key) {
            "pbm_transcranial" -> {
                val lim = NPPBMTranscranialLimits()
                fields["max_intensity"]?.asPercent?.let { lim.maxIntensityPercent = it }
                fields["max_frequency"]?.asHz?.let { lim.maxFrequencyHz = it }
                fields["max_duty_cycle"]?.asPercent?.let { lim.maxDutyCyclePercent = it.toInt() }
                fields["max_session_dose"]?.asDouble?.let { lim.maxSessionDoseJCm2 = it }
                fields["max_daily_dose"]?.asDouble?.let { lim.maxDailyDoseJCm2 = it }
                limitsSet.pbmTranscranial = lim
            }
            "pbm_intranasal" -> {
                val lim = NPPBMIntranasalLimits()
                fields["max_intensity"]?.asPercent?.let { lim.maxIntensityPercent = it }
                fields["max_session_dose"]?.asDouble?.let { lim.maxSessionDoseJCm2 = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                limitsSet.pbmIntranasal = lim
            }
            "eeg_neurofeedback" -> {
                val lim = NPEEGNeurofeedbackLimits()
                stringList(fields["allowed_bands"])?.let { lim.allowedBands = it }
                fields["require_closed_loop"]?.asBool?.let { lim.requireClosedLoop = it }
                limitsSet.eegNeurofeedback = lim
            }
            "bes_tacs" -> {
                val lim = NPBESTacsLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_frequency"]?.asHz?.let { lim.maxFrequencyHz = it }
                fields["min_frequency"]?.asHz?.let { lim.minFrequencyHz = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                fields["max_sessions_per_day"]?.asDouble?.let { lim.maxSessionsPerDay = it.toInt() }
                limitsSet.besTacs = lim
            }
            "tdcs" -> {
                val lim = NPTDCSLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                fields["max_sessions_per_day"]?.asDouble?.let { lim.maxSessionsPerDay = it.toInt() }
                limitsSet.tdcs = lim
            }
            "vns_hrv" -> {
                val lim = NPVNSHRVLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_frequency"]?.asHz?.let { lim.maxFrequencyHz = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                stringList(fields["allowed_protocols"])?.let { lim.allowedProtocols = it }
                limitsSet.vnsHrv = lim
            }
            "audio_entrainment" -> {
                val lim = NPAudioEntrainmentLimits()
                fields["max_volume"]?.asPercent?.let { lim.maxVolumePercent = it }
                fields["max_binaural_hz"]?.asHz?.let { lim.maxBinauralBeatsHz = it }
                fields["max_isochronic_hz"]?.asHz?.let { lim.maxIsochronicTonesHz = it }
                limitsSet.audioEntrainment = lim
            }
            "visual_stimulation" -> {
                val lim = NPVisualStimLimits()
                fields["max_frequency"]?.asHz?.let { lim.maxFrequencyHz = it }
                fields["min_frequency"]?.asHz?.let { lim.minFrequencyHz = it }
                fields["block_high_risk_range"]?.asBool?.let { lim.blockHighRiskRange = it }
                stringList(fields["allowed_modes"])?.let { lim.allowedModes = it }
                limitsSet.visualStimulation = lim
            }
            "tms" -> {
                val lim = NPTMSLimits()
                fields["max_intensity_mt"]?.asDouble?.let { lim.maxIntensityPercentMT = it.toInt() }
                fields["max_pulses_per_session"]?.asDouble?.let { lim.maxPulsesPerSession = it.toInt() }
                fields["max_pulses_per_day"]?.asDouble?.let { lim.maxPulsesPerDay = it.toInt() }
                fields["max_sessions_per_week"]?.asDouble?.let { lim.maxSessionsPerWeek = it.toInt() }
                stringList(fields["allowed_protocols"])?.let { lim.allowedProtocols = it }
                stringList(fields["allowed_targets"])?.let { lim.allowedTargets = it }
                limitsSet.tms = lim
            }
            "pbm_deep_1170nm" -> {
                val lim = NPDeepPBMLimits()
                fields["max_intensity"]?.asDouble?.let { lim.maxIntensityMWcm2 = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                limitsSet.pbmDeep1170nm = lim
            }
            "clinical_tacs" -> {
                val lim = NPClinicalTacsLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                limitsSet.clinicalTacs = lim
            }
            "hd_tdcs" -> {
                val lim = NPHDTdcsLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                stringList(fields["allowed_montages"])?.let { lim.allowedMontages = it }
                limitsSet.hdTdcs = lim
            }
            "cervical_vns" -> {
                val lim = NPCervicalVnsLimits()
                fields["max_intensity"]?.asMilliamps?.let { lim.maxIntensityMilliamps = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                limitsSet.cervicalVns = lim
            }
            "vibrotactile_40hz" -> {
                val lim = NPVibrotactileLimits()
                fields["max_intensity_g"]?.asDouble?.let { lim.maxIntensityG = it }
                fields["max_session_duration"]?.asTime?.let { lim.maxSessionDurationSeconds = it }
                limitsSet.vibrotactile40hz = lim
            }
            else -> {
                // Forward compatibility: unknown modality limits blocks are ignored.
            }
        }
    }

    // MARK: Protocol ---------------------------------------------------------

    private fun parseProtocol(): NPProtocolDefinition {
        val ln = currentLine()
        expectKeyword("protocol")
        val name = parseString()
        expect(NPPSToken.LBrace)

        var id: UUID = UUID.randomUUID()
        var description = ""
        var author = "NeurOne"
        var version = "1.0"
        var tags: List<String> = emptyList()
        var timingMode: NPTimingMode = NPTimingMode.Duration(20 * 60)
        val modalities = ArrayList<NPProtocolModality>()
        var isReadOnly = false

        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val keyTok = currentToken()
            if (keyTok !is NPPSToken.Ident) {
                throw NPPSError("Expected field name or modality block", currentLine())
            }
            val key = keyTok.value
            advance()

            when (currentToken()) {
                is NPPSToken.Colon -> {
                    advance()
                    when (key) {
                        "id" -> {
                            val uuidStr = parseString()
                            parseUuidOrNull(uuidStr)?.let { id = it }
                        }
                        "readonly" -> {
                            val t = currentToken()
                            if (t is NPPSToken.BoolTok) { isReadOnly = t.value; advance() }
                        }
                        "description" -> description = parseString()
                        "author" -> author = parseString()
                        "version" -> version = parseString()
                        "tags" -> tags = parseTagList()
                        "duration" -> timingMode = NPTimingMode.Duration(parseTimeValue())
                        "interval_count" -> {
                            val t = currentToken()
                            when (t) {
                                is NPPSToken.Num -> {
                                    timingMode = NPTimingMode.IntervalCount(t.value.toInt()); advance()
                                }
                                is NPPSToken.NumberWithUnit -> {
                                    timingMode = NPTimingMode.IntervalCount(t.value.toInt()); advance()
                                }
                                else -> throw NPPSError(
                                    "Expected number for interval_count", currentLine()
                                )
                            }
                        }
                        else -> skipValue()
                    }
                }
                is NPPSToken.LBrace -> modalities.add(parseModalityBlock(key))
                else -> throw NPPSError("Expected ':' or '{' after '$key'", currentLine())
            }
        }

        expect(NPPSToken.RBrace)
        if (name.isEmpty()) throw NPPSError("Protocol name cannot be empty", ln)

        return NPProtocolDefinition(
            id = id,
            name = name,
            description = description,
            author = author,
            version = version,
            tags = tags,
            isPredefined = isReadOnly,
            isReadOnly = isReadOnly,
            timingMode = timingMode,
            modalities = modalities,
        )
    }

    // MARK: Composite --------------------------------------------------------

    private fun parseComposite(): NPCompositeProtocol {
        val ln = currentLine()
        expectKeyword("composite")
        val name = parseString()
        expect(NPPSToken.LBrace)

        var id: UUID = UUID.randomUUID()
        var description = ""
        var author = "NeurOne"
        var version = "1.0"
        var tags: List<String> = emptyList()
        var conflictResolution = NPCompositeProtocol.ConflictResolution.MERGE
        val layers = ArrayList<NPCompositeLayer>()
        var isReadOnly = false

        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val cur = currentToken()
            // "layer" keyword (emitted as Keyword("layer") by the lexer).
            if (cur is NPPSToken.Keyword && cur.value == "layer") {
                advance()
                layers.add(parseLayerBlock())
                continue
            }

            if (cur !is NPPSToken.Ident) {
                throw NPPSError("Expected field name in composite block", currentLine())
            }
            val key = cur.value
            advance()

            if (currentToken() is NPPSToken.Colon) {
                advance()
                when (key) {
                    "id" -> {
                        val uuidStr = parseString()
                        parseUuidOrNull(uuidStr)?.let { id = it }
                    }
                    "readonly" -> {
                        val t = currentToken()
                        if (t is NPPSToken.BoolTok) { isReadOnly = t.value; advance() }
                    }
                    "description" -> description = parseString()
                    "author" -> author = parseString()
                    "version" -> version = parseString()
                    "conflict_resolution" -> {
                        when (parseIdentOrString()) {
                            "merge" -> conflictResolution = NPCompositeProtocol.ConflictResolution.MERGE
                            "sequential" -> conflictResolution = NPCompositeProtocol.ConflictResolution.SEQUENTIAL
                            "override" -> conflictResolution = NPCompositeProtocol.ConflictResolution.OVERRIDE
                            else -> { /* unknown ignored */ }
                        }
                    }
                    "tags" -> tags = parseTagList()
                    else -> skipValue()
                }
            } else {
                throw NPPSError(
                    "Unexpected token after '$key' in composite block", currentLine()
                )
            }
        }

        expect(NPPSToken.RBrace)
        if (name.isEmpty()) throw NPPSError("Composite name cannot be empty", ln)

        return NPCompositeProtocol(
            id = id,
            name = name,
            description = description,
            author = author,
            version = version,
            tags = tags,
            isPredefined = isReadOnly,
            isReadOnly = isReadOnly,
            layers = layers,
            conflictResolution = conflictResolution,
        )
    }

    // MARK: Layer block (called after "layer" keyword consumed) --------------

    private fun parseLayerBlock(): NPCompositeLayer {
        val protocolName = parseString()
        expect(NPPSToken.LBrace)

        var startOffsetSeconds = 0
        var durationSeconds: Int? = null
        var intensityScale = 1.0

        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val keyTok = currentToken()
            if (keyTok !is NPPSToken.Ident) {
                throw NPPSError("Expected field name in layer block", currentLine())
            }
            val key = keyTok.value
            advance()
            expect(NPPSToken.Colon)
            when (key) {
                "start" -> startOffsetSeconds = parseTimeValue()
                "duration" -> durationSeconds = parseTimeValue()
                "intensity_scale" -> intensityScale = parseDoubleValue()
                else -> skipValue()
            }
        }

        expect(NPPSToken.RBrace)

        return NPCompositeLayer(
            protocolName = protocolName,
            startOffsetSeconds = startOffsetSeconds,
            durationSeconds = durationSeconds,
            intensityScale = intensityScale,
        )
    }

    // MARK: Modality block ---------------------------------------------------

    private fun parseModalityBlock(name: String): NPProtocolModality {
        expect(NPPSToken.LBrace)

        val fields = HashMap<String, NPPSFieldValue>()
        while (currentToken() !is NPPSToken.RBrace && currentToken() !is NPPSToken.Eof) {
            val keyTok = currentToken()
            if (keyTok !is NPPSToken.Ident) {
                throw NPPSError("Expected field name in modality block", currentLine())
            }
            advance()
            expect(NPPSToken.Colon)
            fields[keyTok.value] = parseFieldValue()
        }
        expect(NPPSToken.RBrace)

        val intervalOn = fields["interval_on"]?.asTime ?: 0
        val intervalOff = fields["interval_off"]?.asTime ?: 0
        var repeatCount: Int? = null
        when (val rv = fields["repeat"]) {
            is NPPSFieldValue.Ident -> if (rv.value == "until_end") repeatCount = null
            is NPPSFieldValue.Num -> repeatCount = rv.value.toInt()
            is NPPSFieldValue.NumberWithUnit -> repeatCount = rv.value.toInt()
            else -> { /* no repeat / other → null */ }
        }
        val interval = NPIntervalConfig(
            intervalOnSeconds = intervalOn,
            intervalOffSeconds = intervalOff,
            repeatCount = repeatCount,
        )

        val params = buildParams(name, fields)
        return NPProtocolModality(params = params, interval = interval, enabled = true)
    }

    // MARK: Params builder ---------------------------------------------------

    private fun buildParams(name: String, fields: Map<String, NPPSFieldValue>): NPModalityParams {
        when (name) {
            "pbm_transcranial" -> {
                val p = NPPBMTranscranialParams()
                fields["intensity"]?.asPercent?.let { p.intensityPercent = it }
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["duty_cycle"]?.asPercent?.let { p.dutyCyclePercent = it.toInt() }
                when (val v = fields["zones"]) {
                    is NPPSFieldValue.Ident -> p.zones = when (v.value) {
                        "all" -> NPPBMTranscranialParams.ZoneSelection.ALL
                        "front" -> NPPBMTranscranialParams.ZoneSelection.FRONT
                        "rear" -> NPPBMTranscranialParams.ZoneSelection.REAR
                        else -> NPPBMTranscranialParams.ZoneSelection.ALL
                    }
                    is NPPSFieldValue.Arr -> {
                        p.zones = NPPBMTranscranialParams.ZoneSelection.CUSTOM
                        p.customZones = v.items.mapNotNull { item ->
                            (item as? NPPSFieldValue.Num)?.let { it.value.toInt() - 1 } // 1-indexed in script
                        }
                    }
                    else -> { /* absent */ }
                }
                fields["wavelength"]?.asIdent?.let { w ->
                    p.wavelength = when (w) {
                        "660_808nm" -> NPPBMTranscranialParams.Wavelength.BASE_660_808NM
                        "1064nm" -> NPPBMTranscranialParams.Wavelength.SMART_1064NM
                        "660_808_1064nm" -> NPPBMTranscranialParams.Wavelength.TRI_660_808_1064
                        else -> NPPBMTranscranialParams.Wavelength.BASE_660_808NM
                    }
                }
                return NPModalityParams.PbmTranscranial(p)
            }

            "pbm_intranasal" -> {
                val p = NPPBMIntranasalParams()
                fields["intensity"]?.asPercent?.let { p.intensityPercent = it }
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["duty_cycle"]?.asPercent?.let { p.dutyCyclePercent = it.toInt() }
                return NPModalityParams.PbmIntranasal(p)
            }

            "eeg_neurofeedback" -> {
                val p = NPEEGNeurofeedbackParams()
                fields["band"]?.asIdent?.let { b ->
                    when (b) {
                        "delta" -> p.band = NPEEGNeurofeedbackParams.EEGBand.DELTA
                        "theta" -> p.band = NPEEGNeurofeedbackParams.EEGBand.THETA
                        "alpha" -> p.band = NPEEGNeurofeedbackParams.EEGBand.ALPHA
                        "beta" -> p.band = NPEEGNeurofeedbackParams.EEGBand.BETA
                        "gamma" -> p.band = NPEEGNeurofeedbackParams.EEGBand.GAMMA
                        "alpha_theta" -> p.band = NPEEGNeurofeedbackParams.EEGBand.ALPHA_THETA
                        "gamma_theta" -> p.band = NPEEGNeurofeedbackParams.EEGBand.GAMMA_THETA
                        else -> { /* unknown ignored */ }
                    }
                }
                when (val v = fields["channels"]) {
                    is NPPSFieldValue.Ident -> p.channels = when (v.value) {
                        "all" -> NPEEGNeurofeedbackParams.ChannelSelection.ALL
                        "front" -> NPEEGNeurofeedbackParams.ChannelSelection.FRONT
                        "central" -> NPEEGNeurofeedbackParams.ChannelSelection.CENTRAL
                        else -> NPEEGNeurofeedbackParams.ChannelSelection.ALL
                    }
                    is NPPSFieldValue.Arr -> {
                        p.channels = NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM
                        p.customChannels = v.items.mapNotNull { (it as? NPPSFieldValue.Str)?.value }
                    }
                    else -> { /* absent */ }
                }
                fields["closed_loop"]?.asBool?.let { p.closedLoopEnabled = it }
                return NPModalityParams.EegNeurofeedback(p)
            }

            "bes_tacs" -> {
                val p = NPBESTacsParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                fields["waveform"]?.asIdent?.let { w ->
                    when (w) {
                        "sinusoidal" -> p.waveform = NPBESTacsParams.Waveform.SINUSOIDAL
                        "square" -> p.waveform = NPBESTacsParams.Waveform.SQUARE
                        "triangular" -> p.waveform = NPBESTacsParams.Waveform.TRIANGULAR
                        else -> { /* unknown ignored */ }
                    }
                }
                return NPModalityParams.BesTacs(p)
            }

            "tdcs" -> {
                val p = NPTDCSParams()
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                return NPModalityParams.Tdcs(p)
            }

            "vns_hrv" -> {
                val p = NPVNSHRVParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                fields["hrv_protocol"]?.asIdent?.let { h ->
                    when (h) {
                        "standalone" -> p.hrvProtocol = NPVNSHRVParams.HRVProtocol.STANDALONE
                        "tavns_sync" -> p.hrvProtocol = NPVNSHRVParams.HRVProtocol.TAVNS_SYNC
                        "eeg_biofeedback" -> p.hrvProtocol = NPVNSHRVParams.HRVProtocol.EEG_BIOFEEDBACK
                        "combined_pbm" -> p.hrvProtocol = NPVNSHRVParams.HRVProtocol.COMBINED_PBM
                        else -> { /* unknown ignored */ }
                    }
                }
                fields["breathing_rate"]?.asDouble?.let { p.resonanceBreathingRate = it }
                return NPModalityParams.VnsHRV(p)
            }

            "audio_entrainment" -> {
                val p = NPAudioEntrainmentParams()
                fields["binaural_hz"]?.asHz?.let { p.binauralBeatsHz = it }
                fields["isochronic_hz"]?.asHz?.let { p.isochronicTonesHz = it }
                fields["noise"]?.asIdent?.let { n ->
                    when (n) {
                        "pink" -> p.noiseType = NPAudioEntrainmentParams.NoiseType.PINK
                        "brown" -> p.noiseType = NPAudioEntrainmentParams.NoiseType.BROWN
                        "none" -> p.noiseType = null
                        else -> { /* unknown ignored */ }
                    }
                }
                fields["carrier_hz"]?.asHz?.let { p.carrierHz = it }
                fields["volume"]?.asPercent?.let { p.volumePercent = it }
                fields["eeg_adaptive"]?.asBool?.let { p.eegAdaptive = it }
                fields["bone_conduction_pacer"]?.asBool?.let { p.boneConductionPacer = it }
                return NPModalityParams.AudioEntrainment(p)
            }

            "visual_stimulation" -> {
                val p = NPVisualStimParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["mode"]?.asIdent?.let { m ->
                    when (m) {
                        "binocular" -> p.mode = NPVisualStimParams.VisualMode.BINOCULAR
                        "emdr" -> p.mode = NPVisualStimParams.VisualMode.EMDR
                        "retinal_pbm" -> p.mode = NPVisualStimParams.VisualMode.RETINAL_PBM
                        "mode_f" -> p.mode = NPVisualStimParams.VisualMode.MODE_F
                        else -> { /* unknown ignored */ }
                    }
                }
                fields["emdr_cadence"]?.asHz?.let { p.emdrCadenceHz = it }
                fields["mode_f"]?.asBool?.let { p.enableModeF = it }
                return NPModalityParams.VisualStimulation(p)
            }

            "qeeg_21ch" -> {
                val p = NPqEEG21chParams()
                fields["sloreta"]?.asBool?.let { p.sloretaEnabled = it }
                fields["reference"]?.asIdent?.let { r ->
                    when (r) {
                        "linked_ear" -> p.reference = NPqEEG21chParams.Reference.LINKED_EAR
                        "cz" -> p.reference = NPqEEG21chParams.Reference.CZ
                        "average" -> p.reference = NPqEEG21chParams.Reference.AVERAGE
                        else -> { /* unknown ignored */ }
                    }
                }
                return NPModalityParams.Qeeg21ch(p)
            }

            "tms" -> {
                val p = NPTMSParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["intensity_mt"]?.asDouble?.let { p.intensityPercentMT = it.toInt() }
                fields["pulse_count"]?.asDouble?.let { p.pulseCount = it.toInt() }
                fields["target"]?.asIdent?.let { t ->
                    NPTMSParams.TMSTarget.fromRawValue(t.uppercase())?.let { p.target = it }
                }
                fields["protocol"]?.asIdent?.let { pr ->
                    when (pr) {
                        "rTMS" -> p.tmsProtocol = NPTMSParams.TMSProtocol.RTMS
                        "TBS" -> p.tmsProtocol = NPTMSParams.TMSProtocol.TBS
                        "iTBS" -> p.tmsProtocol = NPTMSParams.TMSProtocol.ITBS
                        else -> { /* unknown ignored */ }
                    }
                }
                return NPModalityParams.Tms(p)
            }

            "pbm_deep_1170nm" -> {
                val p = NPDeepPBM1170Params()
                fields["intensity"]?.asDouble?.let { p.intensityMWcm2 = it }
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["duty_cycle"]?.asPercent?.let { p.dutyCyclePercent = it.toInt() }
                return NPModalityParams.PbmDeep1170nm(p)
            }

            "clinical_tacs" -> {
                val p = NPClinicalTacsParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                fields["channels"]?.asDouble?.let { p.channelCount = it.toInt() }
                return NPModalityParams.ClinicalTacs(p)
            }

            "hd_tdcs" -> {
                val p = NPHDTdcsParams()
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                fields["montage"]?.asIdent?.let { m ->
                    when (m) {
                        "ring_4x1", "4x1_ring" -> p.montage = NPHDTdcsParams.Montage.RING_4X1
                        "bilateral_4x1" -> p.montage = NPHDTdcsParams.Montage.BILATERAL_4X1
                        "standard_2_electrode" -> p.montage = NPHDTdcsParams.Montage.STANDARD_2EL
                        else -> { /* unknown ignored */ }
                    }
                }
                fields["target"]?.asIdent?.let { t ->
                    NPTMSParams.TMSTarget.fromRawValue(t.uppercase())?.let { p.target = it }
                }
                return NPModalityParams.HdTdcs(p)
            }

            "cervical_vns" -> {
                val p = NPCervicalVnsParams()
                fields["frequency"]?.asHz?.let { p.frequencyHz = it }
                fields["intensity"]?.asMilliamps?.let { p.intensityMilliamps = it }
                return NPModalityParams.CervicalVns(p)
            }

            "vibrotactile_40hz" -> {
                val p = NPVibrotactileParams()
                fields["intensity_g"]?.asDouble?.let { p.intensityG = it }
                fields["sync_audio"]?.asBool?.let { p.syncToAudio = it }
                fields["sync_visual"]?.asBool?.let { p.syncToVisual = it }
                return NPModalityParams.Vibrotactile40hz(p)
            }

            else -> throw NPPSError("Unknown modality: $name", 0)
        }
    }

    // MARK: Field value parser ----------------------------------------------

    private fun parseFieldValue(): NPPSFieldValue {
        return when (val t = currentToken()) {
            is NPPSToken.Num -> { advance(); NPPSFieldValue.Num(t.value) }
            is NPPSToken.NumberWithUnit -> { advance(); NPPSFieldValue.NumberWithUnit(t.value, t.unit) }
            is NPPSToken.Str -> { advance(); NPPSFieldValue.Str(t.value) }
            is NPPSToken.BoolTok -> { advance(); NPPSFieldValue.BoolVal(t.value) }
            is NPPSToken.Ident -> { advance(); NPPSFieldValue.Ident(t.value) }
            is NPPSToken.LBracket -> {
                advance()
                val items = ArrayList<NPPSFieldValue>()
                while (currentToken() !is NPPSToken.RBracket && currentToken() !is NPPSToken.Eof) {
                    items.add(parseFieldValue())
                    if (currentToken() is NPPSToken.Comma) advance()
                }
                expect(NPPSToken.RBracket)
                NPPSFieldValue.Arr(items)
            }
            else -> throw NPPSError("Expected a value", currentLine())
        }
    }

    // MARK: Helpers ----------------------------------------------------------

    private fun currentToken(): NPPSToken = tokens[pos].token
    private fun currentLine(): Int = tokens[pos].line

    private fun advance() {
        if (pos < tokens.size - 1) pos += 1
    }

    private fun expect(token: NPPSToken) {
        // Swift compared enum cases (with associated values) by value equality.
        // The delimiter/eof tokens we expect() on are singletons or value
        // data-classes, so `==` reproduces Swift semantics exactly.
        if (currentToken() == token) {
            advance()
        } else {
            throw NPPSError(
                "Expected ${describe(token)} but got ${describe(currentToken())}", currentLine()
            )
        }
    }

    private fun expectKeyword(kw: String) {
        val t = currentToken()
        if (t is NPPSToken.Keyword && t.value == kw) {
            advance()
        } else {
            throw NPPSError("Expected keyword '$kw'", currentLine())
        }
    }

    private fun parseString(): String {
        return when (val t = currentToken()) {
            is NPPSToken.Str -> { advance(); t.value }
            is NPPSToken.Ident -> { advance(); t.value }
            else -> throw NPPSError("Expected string", currentLine())
        }
    }

    private fun parseIdentOrString(): String = parseString()

    private fun parseTagList(): List<String> {
        expect(NPPSToken.LBracket)
        val tags = ArrayList<String>()
        while (currentToken() !is NPPSToken.RBracket && currentToken() !is NPPSToken.Eof) {
            when (val t = currentToken()) {
                is NPPSToken.Ident -> { tags.add(t.value); advance() }
                is NPPSToken.Str -> { tags.add(t.value); advance() }
                else -> advance()
            }
            if (currentToken() is NPPSToken.Comma) advance()
        }
        expect(NPPSToken.RBracket)
        return tags
    }

    private fun parseTimeValue(): Int {
        val fv = parseFieldValue()
        return fv.asTime ?: throw NPPSError(
            "Expected a time value (e.g. 20m, 30s, 1h)", currentLine()
        )
    }

    private fun parseDoubleValue(): Double {
        val fv = parseFieldValue()
        return fv.asDouble ?: throw NPPSError("Expected a numeric value", currentLine())
    }

    private fun skipValue() {
        if (currentToken() is NPPSToken.LBracket) {
            advance()
            var depth = 1
            while (currentToken() !is NPPSToken.Eof && depth > 0) {
                if (currentToken() is NPPSToken.LBracket) depth += 1
                if (currentToken() is NPPSToken.RBracket) depth -= 1
                advance()
            }
        } else {
            advance()
        }
    }

    private fun describe(token: NPPSToken): String = when (token) {
        is NPPSToken.Keyword -> "keyword(${token.value})"
        is NPPSToken.Ident -> "ident(${token.value})"
        is NPPSToken.Str -> "string(${token.value})"
        is NPPSToken.Num -> "number(${token.value})"
        is NPPSToken.NumberWithUnit -> "numberWithUnit(${token.value}, ${token.unit})"
        is NPPSToken.BoolTok -> "bool(${token.value})"
        NPPSToken.LBrace -> "{"
        NPPSToken.RBrace -> "}"
        NPPSToken.LBracket -> "["
        NPPSToken.RBracket -> "]"
        NPPSToken.Colon -> ":"
        NPPSToken.Comma -> ","
        NPPSToken.Eof -> "eof"
    }
}
