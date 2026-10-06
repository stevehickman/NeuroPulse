import Foundation

// The lexer and parser that used to live here are the shared NPPS core now (NPPSParser.swift,
// common/npps-core, OI-NPPS-CORE-01). What is left in this file is the serializer.

// MARK: - NPPS Serializer

struct NPPSSerializer {

    func serialize(_ entry: NPProtocolEntry) -> String {
        switch entry {
        case .single(let proto):
            return serializeProtocol(proto)
        case .composite(let comp):
            return serializeComposite(comp)
        case .limits(let lim):
            return serializeLimits(lim)
        case .zone(let zone):
            return serializeZone(zone)
        case .condition(let cond):
            return serializeCondition(cond)
        }
    }

    /// A bare URL, or a `[label, url]` pair when the entry was labelled.
    private func serializeReference(_ r: NPProtocolReference) -> String {
        guard let label = r.label else { return "\"\(escape(r.url))\"" }
        return "[\"\(escape(label))\", \"\(escape(r.url))\"]"
    }

    /// Escape a value for a double-quoted NPPS string.
    private func escape(_ value: String) -> String {
        value.replacingOccurrences(of: "\\", with: "\\\\")
             .replacingOccurrences(of: "\"", with: "\\\"")
    }

    // MARK: Zone / Condition (NP-NPPS-REF-001 §8, §9)

    func serializeZone(_ z: NPZoneDefinition) -> String {
        var lines: [String] = ["zone \"\(escape(z.name))\" {"]
        if let id = z.id { lines.append("    id: \"\(escape(id))\"") }
        if let d = z.description { lines.append("    description: \"\(escape(d))\"") }
        // Canonical membership on the way out as well as in: sorted and deduped,
        // so two equal zones serialize identically.
        let sockets = Array(Set(z.sockets)).sorted().map(String.init).joined(separator: ", ")
        lines.append("    sockets: [\(sockets)]")
        if let types = z.types, !types.isEmpty {
            lines.append("    types: [\(types.joined(separator: ", "))]")
        }
        if z.excludeTypes { lines.append("    exclude_types: true") }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    func serializeCondition(_ c: NPConditionDefinition) -> String {
        var lines: [String] = ["condition \"\(escape(c.name))\" {"]
        if let id = c.id { lines.append("    id: \"\(escape(id))\"") }
        lines.append("    link: \"\(escape(c.link))\"")
        if let code = c.code { lines.append("    code: \"\(escape(code))\"") }
        if let d = c.description { lines.append("    description: \"\(escape(d))\"") }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    func serializeLimits(_ limits: NPLimitsSet) -> String {
        var lines: [String] = []
        lines.append("limits \"\(limits.name)\" {")
        lines.append("    level: \(limits.level.rawValue)")
        if let hid = limits.helmetId { lines.append("    helmet_id: \"\(hid)\"") }
        if let iid = limits.individualId { lines.append("    individual_id: \"\(iid.uuidString)\"") }
        if !limits.description.isEmpty { lines.append("    description: \"\(limits.description)\"") }
        lines.append("")

        if let lim = limits.pbmTranscranial {
            lines.append("    pbm_transcranial {")
            if let v = lim.maxIrradianceMWcm2    { lines.append("        max_irradiance_mw_cm2: \(v)") }
            if let v = lim.maxFrequencyHz         { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.maxDutyCyclePercent    { lines.append("        max_duty_cycle: \(v)%") }
            if let v = lim.maxSessionDoseJCm2     { lines.append("        max_session_dose: \(v)") }
            if let v = lim.maxDailyDoseJCm2       { lines.append("        max_daily_dose: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.pbmIntranasal {
            lines.append("    pbm_intranasal {")
            if let v = lim.maxIrradianceMWcm2         { lines.append("        max_irradiance_mw_cm2: \(v)") }
            if let v = lim.maxSessionDoseJCm2         { lines.append("        max_session_dose: \(v)") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.eegNeurofeedback {
            lines.append("    eeg_neurofeedback {")
            if let v = lim.allowedBands       { lines.append("        allowed_bands: [\(v.joined(separator: ", "))]") }
            if let v = lim.requireClosedLoop  { lines.append("        require_closed_loop: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.besTacs {
            lines.append("    bes_tacs {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxFrequencyHz             { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.minFrequencyHz             { lines.append("        min_frequency: \(formatHz(v))") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.maxSessionsPerDay          { lines.append("        max_sessions_per_day: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.tdcs {
            lines.append("    tdcs {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.maxSessionsPerDay          { lines.append("        max_sessions_per_day: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.vnsHrv {
            lines.append("    vns_hrv {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxFrequencyHz             { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.allowedProtocols           { lines.append("        allowed_protocols: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.audioEntrainment {
            lines.append("    audio_entrainment {")
            if let v = lim.maxVolumeDb            { lines.append("        max_volume_db: \(v)") }
            if let v = lim.maxBinauralBeatsHz     { lines.append("        max_binaural_beats: \(formatHz(v))") }
            if let v = lim.maxIsochronicTonesHz   { lines.append("        max_isochronic_tones: \(formatHz(v))") }
            lines.append("    }")
        }
        if let lim = limits.visualStimulation {
            lines.append("    visual_stimulation {")
            if let v = lim.maxFrequencyHz         { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.minFrequencyHz         { lines.append("        min_frequency: \(formatHz(v))") }
            if let v = lim.allowedModes           { lines.append("        allowed_modes: [\(v.joined(separator: ", "))]") }
            if let v = lim.blockHighRiskRange     { lines.append("        block_high_risk_range: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.tms {
            lines.append("    tms {")
            if let v = lim.maxIntensityPercentMT  { lines.append("        max_intensity_pct_mt: \(v)") }
            if let v = lim.maxPulsesPerSession    { lines.append("        max_pulses_per_session: \(v)") }
            if let v = lim.maxPulsesPerDay        { lines.append("        max_pulses_per_day: \(v)") }
            if let v = lim.maxSessionsPerWeek     { lines.append("        max_sessions_per_week: \(v)") }
            if let v = lim.allowedProtocols       { lines.append("        allowed_protocols: [\(v.joined(separator: ", "))]") }
            if let v = lim.allowedTargets         { lines.append("        allowed_targets: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.pbmDeep1170nm {
            lines.append("    pbm_deep_1170nm {")
            if let v = lim.maxIntensityMWcm2          { lines.append("        max_intensity: \(v)mW_cm2") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.clinicalTacs {
            lines.append("    clinical_tacs {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.hdTdcs {
            lines.append("    hd_tdcs {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.allowedMontages             { lines.append("        allowed_montages: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.cervicalVns {
            lines.append("    cervical_vns {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.vibrotactile40hz {
            lines.append("    vibrotactile_40hz {")
            if let v = lim.maxIntensityG               { lines.append("        max_intensity: \(v)") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }

        lines.append("}")
        return lines.joined(separator: "\n")
    }

    private func serializeProtocol(_ proto: NPProtocolDefinition) -> String {
        var lines: [String] = []
        lines.append("protocol \"\(proto.name)\" {")
        lines.append("    id: \"\(proto.id.uuidString)\"")
        if !proto.description.isEmpty {
            lines.append("    description: \"\(proto.description)\"")
        }
        if proto.author != "NeurOne" {
            lines.append("    author: \"\(proto.author)\"")
        }
        lines.append("    version: \"\(proto.version)\"")
        if proto.isReadOnly {
            lines.append("    readonly: true")
        }
        if !proto.tags.isEmpty {
            lines.append("    tags: [\(proto.tags.map { serializeTag($0) }.joined(separator: ", "))]")
        }
        if !proto.conditions.isEmpty {
            let cs = proto.conditions.map { "\"\(escape($0))\"" }.joined(separator: ", ")
            lines.append("    conditions: [\(cs)]")
        }
        if !proto.references.isEmpty {
            let rs = proto.references.map { serializeReference($0) }.joined(separator: ", ")
            lines.append("    references: [\(rs)]")
        }
        switch proto.timingMode {
        case .duration(let s):
            lines.append("    duration: \(formatTime(s))")
        case .intervalCount(let n):
            lines.append("    interval_count: \(n)")
        }
        lines.append("")
        for mod in proto.modalities {
            if !mod.enabled { lines.append("    # (disabled)") }
            lines.append(contentsOf: serializeModality(mod).map { "    \($0)" })
            lines.append("")
        }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    private func serializeModality(_ mod: NPProtocolModality) -> [String] {
        var lines: [String] = []
        lines.append("\(mod.modalityType.rawValue) {")
        lines.append(contentsOf: serializeParams(mod.params).map { "    \($0)" })
        if let start = mod.interval.startOffsetSeconds, start > 0 {
            lines.append("    start: \(formatTime(start))")
        }
        if !mod.interval.isContinuous {
            lines.append("    interval_on: \(formatTime(mod.interval.intervalOnSeconds))")
            lines.append("    interval_off: \(formatTime(mod.interval.intervalOffSeconds))")
            if let rc = mod.interval.repeatCount {
                lines.append("    repeat: \(rc)")
            } else {
                lines.append("    repeat: until_end")
            }
        }
        lines.append("}")
        return lines
    }

    private func serializeParams(_ params: NPModalityParams) -> [String] {
        switch params {
        case .pbmTranscranial(let p):
            var lines: [String] = []
            lines.append("wavelength: \"\(p.wavelength.rawValue)\"")
            lines.append("irradiance: \(p.irradianceMWcm2)mW_cm2")
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            if p.frequencyHz > 0 {
                lines.append("duty_cycle: \(p.dutyCyclePercent)%")
            }
            switch p.target {
            case .named(let names):
                let refs = names.map { "\"\($0)\"" }.joined(separator: ", ")
                lines.append("zones: [\(refs)]")
            case .clinicianSelected:
                lines.append("zones: clinician_selected")
            }
            return lines

        case .pbmIntranasal(let p):
            return [
                "wavelength: \"\(p.wavelength.rawValue)\"",
                "irradiance: \(p.irradianceMWcm2)mW_cm2",
                "frequency: \(formatHz(p.frequencyHz))",
                "duty_cycle: \(p.dutyCyclePercent)%"
            ]

        case .eegNeurofeedback(let p):
            var lines: [String] = []
            lines.append("band: \(p.band.rawValue)")
            switch p.channels {
            case .all:     lines.append("channels: all")
            case .front:   lines.append("channels: front")
            case .central: lines.append("channels: central")
            case .custom:
                let chs = (p.customChannels ?? []).map { "\"\($0)\"" }.joined(separator: ", ")
                lines.append("channels: [\(chs)]")
            }
            lines.append("closed_loop: \(p.closedLoopEnabled)")
            return lines

        case .besTacs(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "waveform: \(p.waveform.rawValue)"
            ]

        case .tdcs(let p):
            let pairs = p.electrodePairs.map { pair in
                "[\(pair.map { "\"\($0)\"" }.joined(separator: ", "))]"
            }.joined(separator: ", ")
            return [
                "intensity: \(p.intensityMilliamps)mA",
                "electrode_pairs: [\(pairs)]",
                "ramp: \(p.rampSeconds)s  # hardware-enforced"
            ]

        case .vnsHRV(let p):
            var lines: [String] = []
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            lines.append("intensity: \(p.intensityMilliamps)mA")
            lines.append("hrv_protocol: \(p.hrvProtocol.rawValue)")
            lines.append("breathing_rate: \(p.resonanceBreathingRate)")
            return lines

        case .audioEntrainment(let p):
            var lines: [String] = []
            if let bb = p.binauralBeatsHz { lines.append("binaural_hz: \(formatHz(bb))") }
            if let it = p.isochronicTonesHz { lines.append("isochronic_hz: \(formatHz(it))") }
            if let nt = p.noiseType { lines.append("noise: \(nt.rawValue)") } else { lines.append("noise: none") }
            lines.append("carrier_hz: \(formatHz(p.carrierHz))")
            lines.append("volume: \(p.volumeDb)dB")
            lines.append("eeg_adaptive: \(p.eegAdaptive)")
            lines.append("bone_conduction_pacer: \(p.boneConductionPacer)")
            return lines

        case .visualStimulation(let p):
            var lines: [String] = []
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            lines.append("mode: \(p.mode.rawValue)")
            if p.mode == .emdr { lines.append("emdr_cadence: \(formatHz(p.emdrCadenceHz))") }
            if p.enableModeF { lines.append("enable_mode_f: true") }
            return lines

        case .qeeg21ch(let p):
            return [
                "montage: \(p.montage.rawValue)",
                "sloreta_enabled: \(p.sloretaEnabled)",
                "reference: \(p.reference.rawValue)"
            ]

        case .tms(let p):
            return [
                "protocol: \(p.tmsProtocol.rawValue)",
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity_percent_mt: \(p.intensityPercentMT)%",
                "target: \(p.target.rawValue)",
                "pulse_count: \(p.pulseCount)"
            ]

        case .pbmDeep1170nm(let p):
            return [
                "intensity: \(p.intensityMWcm2)mW_cm2",
                "frequency: \(formatHz(p.frequencyHz))",
                "duty_cycle: \(p.dutyCyclePercent)%"
            ]

        case .clinicalTacs(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "channel_count: \(p.channelCount)",
                "waveform: \(p.waveform.rawValue)"
            ]

        case .hdTdcs(let p):
            return [
                "target: \(p.target.rawValue)",
                "montage: \(p.montage.rawValue)",
                "intensity: \(p.intensityMilliamps)mA"
            ]

        case .cervicalVns(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "# cardiac interlock always enforced by safety MCU"
            ]

        case .vibrotactile40hz(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))  # locked at 40Hz",
                "intensity_g: \(p.intensityG)",
                "sync_to_audio: \(p.syncToAudio)",
                "sync_to_visual: \(p.syncToVisual)"
            ]
        }
    }

    private func serializeComposite(_ comp: NPCompositeProtocol) -> String {
        var lines: [String] = []
        lines.append("composite \"\(comp.name)\" {")
        lines.append("    id: \"\(comp.id.uuidString)\"")
        if !comp.description.isEmpty {
            lines.append("    description: \"\(comp.description)\"")
        }
        if comp.author != "NeurOne" {
            lines.append("    author: \"\(comp.author)\"")
        }
        lines.append("    version: \"\(comp.version)\"")
        if comp.isReadOnly {
            lines.append("    readonly: true")
        }
        if !comp.tags.isEmpty {
            lines.append("    tags: [\(comp.tags.map { serializeTag($0) }.joined(separator: ", "))]")
        }
        if !comp.conditions.isEmpty {
            let cs = comp.conditions.map { "\"\(escape($0))\"" }.joined(separator: ", ")
            lines.append("    conditions: [\(cs)]")
        }
        if !comp.references.isEmpty {
            let rs = comp.references.map { serializeReference($0) }.joined(separator: ", ")
            lines.append("    references: [\(rs)]")
        }
        lines.append("    conflict_resolution: \(comp.conflictResolution.rawValue)")
        lines.append("")
        for layer in comp.layers {
            lines.append("    layer \"\(layer.protocolName)\" {")
            lines.append("        start: \(formatTime(layer.startOffsetSeconds))")
            if let dur = layer.durationSeconds {
                lines.append("        duration: \(formatTime(dur))")
            }
            if abs(layer.intensityScale - 1.0) > 0.001 {
                lines.append("        intensity_scale: \(layer.intensityScale)")
            }
            lines.append("    }")
            lines.append("")
        }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    // MARK: Formatting helpers

    private func formatTime(_ seconds: Int) -> String {
        if seconds == 0 { return "0s" }
        if seconds % 3600 == 0 { return "\(seconds / 3600)h" }
        if seconds % 60 == 0   { return "\(seconds / 60)m" }
        return "\(seconds)s"
    }

    private func formatHz(_ hz: Double) -> String {
        if hz == 0 { return "0Hz  # CW" }
        if hz == Double(Int(hz)) { return "\(Int(hz))Hz" }
        return "\(hz)Hz"
    }

    // Emit a tag as a bare ident when all characters are letters, digits, or underscores.
    // Quote it otherwise (e.g. "wind-down") so that a serialize→reparse round-trip
    // preserves the original string without silent splitting on hyphens.
    private func serializeTag(_ tag: String) -> String {
        // A bare identifier may not start with a digit: a tag like "1064nm" is
        // all letters and digits but must still be quoted to re-parse (Rev 6).
        let isBareIdent = !tag.isEmpty
            && (tag.first!.isLetter || tag.first! == "_")
            && tag.allSatisfy { $0.isLetter || $0.isNumber || $0 == "_" }
        return isBareIdent ? tag : "\"\(tag.replacingOccurrences(of: "\"", with: "\\\""))\""
    }
}

// MARK: - Round-trip

func nppsRoundTrip(_ text: String) throws -> String {
    var lexer = NPPSLexer(text)
    let tokens = try lexer.tokenize()
    var parser = NPPSParser(tokens)
    let entries = try parser.parse()
    let serializer = NPPSSerializer()
    return entries.map { serializer.serialize($0) }.joined(separator: "\n\n")
}
