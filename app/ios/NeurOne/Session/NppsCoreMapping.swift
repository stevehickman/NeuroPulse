import Foundation

// The iOS models as the shared NPPS core reads them (common/npps/fields.json).
//
// The core takes the normalised protocol shape the web parser produces, so this is the one place the
// iOS models are spelled in it. Two things are deliberate:
//   - Enum values are written the way the field table spells them, not the way these models'
//     `rawValue`s do: `hrvProtocol` is `tavns_sync` there and `tavnsSync` here. The models' spellings
//     are an iOS-local habit, and the core compares strings.
//   - Nothing is validated, clamped or defaulted here. A value that is wrong reaches the core as it
//     is, and the core refuses it with the message every runtime gives.

extension NPProtocolDefinition {

    /// Every zone name a PBM block of this protocol targets by name.
    var nppsNamedZoneRefs: Set<String> {
        var names = Set<String>()
        for modality in modalities {
            if case .pbmTranscranial(let p) = modality.params, case .named(let zones) = p.target {
                names.formUnion(zones)
            }
        }
        return names
    }

    /// The protocol in the shape `NppsCore.compile` takes: `timingMode` and `modalities`.
    func nppsCoreJSON() throws -> Data {
        let timing: [String: Any]
        switch timingMode {
        case .duration(let seconds): timing = ["type": "duration", "seconds": seconds]
        case .intervalCount(let count): timing = ["type": "interval_count", "count": count]
        }
        let blocks: [[String: Any]] = modalities.map { modality in
            let (type, params) = modality.params.nppsCoreParams()
            return [
                "type": type,
                "enabled": modality.enabled,
                "params": params,
                "interval": modality.interval.nppsCoreJSON()
            ]
        }
        return try JSONSerialization.data(withJSONObject: ["timingMode": timing, "modalities": blocks])
    }
}

private extension NPIntervalConfig {
    func nppsCoreJSON() -> [String: Any] {
        var json: [String: Any] = ["intervalOnSeconds": intervalOnSeconds, "intervalOffSeconds": intervalOffSeconds]
        if let repeatCount { json["repeatCount"] = repeatCount }
        if let start = startOffsetSeconds, start > 0 { json["startOffsetSeconds"] = start }
        return json
    }
}

private extension NPVNSHRVParams.HRVProtocol {
    var nppsCoreName: String {
        switch self {
        case .standalone: return "standalone"
        case .tavnsSync: return "tavns_sync"
        case .eegBiofeedback: return "eeg_biofeedback"
        case .combinedPBM: return "combined_pbm"
        }
    }
}

private extension NPEEGNeurofeedbackParams.EEGBand {
    var nppsCoreName: String {
        switch self {
        case .alphaTheta: return "alpha_theta"
        case .gammaTheta: return "gamma_theta"
        default: return rawValue
        }
    }
}

private extension NPModalityParams {
    /// The NPPS modality token and the params the core reads for it.
    func nppsCoreParams() -> (String, [String: Any]) {
        switch self {
        case .pbmTranscranial(let p): return ("pbm_transcranial", p.nppsCoreJSON())
        case .pbmIntranasal(let p):
            return ("pbm_intranasal", [
                "wavelength": p.wavelength.rawValue, "irradianceMWcm2": p.irradianceMWcm2,
                "frequencyHz": p.frequencyHz, "dutyCyclePercent": p.dutyCyclePercent
            ])
        case .eegNeurofeedback(let p):
            var json: [String: Any] = [
                "channels": p.channels.rawValue, "band": p.band.nppsCoreName, "closedLoopEnabled": p.closedLoopEnabled
            ]
            if let custom = p.customChannels { json["customChannels"] = custom }
            return ("eeg_neurofeedback", json)
        case .besTacs(let p):
            return ("bes_tacs", [
                "frequencyHz": p.frequencyHz, "intensityMilliamps": p.intensityMilliamps, "waveform": p.waveform.rawValue
            ])
        case .tdcs(let p):
            return ("tdcs", [
                "intensityMilliamps": p.intensityMilliamps, "electrodePairs": p.electrodePairs,
                "rampSeconds": p.rampSeconds, "electrodeAreaCm2": p.electrodeAreaCm2
            ])
        case .vnsHRV(let p):
            return ("vns_hrv", [
                "frequencyHz": p.frequencyHz, "intensityMilliamps": p.intensityMilliamps,
                "hrvProtocol": p.hrvProtocol.nppsCoreName, "resonanceBreathingRate": p.resonanceBreathingRate
            ])
        case .audioEntrainment(let p):
            var json: [String: Any] = [
                "carrierHz": p.carrierHz, "volumeDb": p.volumeDb, "eegAdaptive": p.eegAdaptive,
                "boneConductionPacer": p.boneConductionPacer
            ]
            if let beats = p.binauralBeatsHz { json["binauralBeatsHz"] = beats }
            if let tones = p.isochronicTonesHz { json["isochronicTonesHz"] = tones }
            if let noise = p.noiseType { json["noiseType"] = noise.rawValue }
            return ("audio_entrainment", json)
        case .visualStimulation(let p):
            return ("visual_stimulation", [
                "frequencyHz": p.frequencyHz, "mode": p.mode.rawValue, "emdrCadenceHz": p.emdrCadenceHz,
                "enableModeF": p.enableModeF
            ])
        case .qeeg21ch(let p):
            return ("qeeg_21ch", [
                "montage": p.montage.rawValue, "sloretaEnabled": p.sloretaEnabled, "reference": p.reference.rawValue
            ])
        case .tms(let p):
            return ("tms", [
                "tmsProtocol": p.tmsProtocol.rawValue, "frequencyHz": p.frequencyHz,
                "intensityPercentMT": p.intensityPercentMT, "target": p.target.rawValue, "pulseCount": p.pulseCount
            ])
        case .pbmDeep1170nm(let p):
            return ("pbm_deep_1170nm", [
                "intensityMWcm2": p.intensityMWcm2, "frequencyHz": p.frequencyHz, "dutyCyclePercent": p.dutyCyclePercent
            ])
        case .clinicalTacs(let p):
            return ("clinical_tacs", [
                "frequencyHz": p.frequencyHz, "intensityMilliamps": p.intensityMilliamps,
                "channelCount": p.channelCount, "waveform": p.waveform.rawValue
            ])
        case .hdTdcs(let p):
            return ("hd_tdcs", [
                "target": p.target.rawValue, "montage": p.montage.rawValue, "intensityMilliamps": p.intensityMilliamps
            ])
        case .cervicalVns(let p):
            return ("cervical_vns", ["frequencyHz": p.frequencyHz, "intensityMilliamps": p.intensityMilliamps])
        case .vibrotactile40hz(let p):
            return ("vibrotactile_40hz", [
                "intensityG": p.intensityG, "syncToAudio": p.syncToAudio, "syncToVisual": p.syncToVisual
            ])
        }
    }
}

private extension NPPBMTranscranialParams {
    func nppsCoreJSON() -> [String: Any] {
        var json: [String: Any] = [
            "wavelength": wavelength.rawValue, "irradianceMWcm2": irradianceMWcm2,
            "frequencyHz": frequencyHz, "dutyCyclePercent": dutyCyclePercent
        ]
        switch target {
        case .named(let zones):
            json["zones"] = "named"
            json["zoneRefs"] = zones
        case .clinicianSelected:
            json["zones"] = "clinician_selected"
        }
        return json
    }
}
