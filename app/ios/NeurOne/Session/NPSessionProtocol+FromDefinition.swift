import Foundation

// Converts an NPProtocolDefinition (the rich app model) to the NPSessionProtocol
// wire format understood by hub firmware. Used by SessionProtocolUploader when
// the caller provides an NPProtocolDefinition rather than a pre-built NPSessionProtocol.

extension NPSessionProtocol {

    /// - Parameter clinicianSockets: operator-chosen 1-based socket ids, needed
    ///   only when a PBM modality targets `clinician_selected`.
    /// - Throws: `NPSocketTargetError` when a PBM target cannot be resolved to
    ///   sockets. Building the wire protocol is the last point at which that can
    ///   be caught, and it must fail rather than substitute a default — the
    ///   validator surfaces the same condition earlier and more readably.
    init(
        from definition: NPProtocolDefinition,
        mode: OperatingMode = .mode2Programming,
        clinicianSockets: [Int]? = nil
    ) throws {
        let durationSeconds = definition.totalDurationSeconds ?? 20 * 60
        var modalities: [ModalityConfig] = []

        for mod in definition.modalities where mod.enabled {
            // The wire has no per-block timing: every config runs from 0. A block with
            // `start` would run at the wrong time, so it is refused, not flattened
            // (NP-NPPS-REF-001 §5; per-block timing lands with OI-AND-WIRE-01's schema).
            if let start = mod.interval.startOffsetSeconds, start > 0 {
                throw NPUnsupportedTimingError()
            }
            switch mod.params {
            case .pbmTranscranial(let p):
                if case .failure(let refusal) = NPWavelengthRules.default.resolveChannels(p.wavelength.rawValue) {
                    throw refusal
                }
                modalities.append(.pbmTranscranial(PBMTranscranialConfig(
                    socketMask: try p.resolveSocketMask(clinicianSockets: clinicianSockets),
                    wavelength: p.wavelength.rawValue,
                    irradianceMWcm2: p.irradianceMWcm2,
                    frequencyHz: p.frequencyHz,
                    dutyCyclePercent: p.dutyCyclePercent,
                    durationSeconds: durationSeconds,
                    // J/cm²: irradiance × duty × time (CW is full duty).
                    targetDoseJoules: p.irradianceMWcm2
                        * (p.frequencyHz == 0 ? 1.0 : Double(p.dutyCyclePercent) / 100.0)
                        * Double(durationSeconds) / 1000.0
                )))
            case .pbmIntranasal(let p):
                // The probe carries the 660 and 808 nm channels only (Rev 18).
                switch NPWavelengthRules.default.resolveChannels(p.wavelength.rawValue) {
                case .failure(let refusal): throw refusal
                case .success(let els):
                    if !els.contains(where: { $0 == .led660 || $0 == .led808 }) {
                        throw NPWavelengthRefusal(value: p.wavelength.rawValue, reason: .unmapped)
                    }
                }
                modalities.append(.pbmIntranasal(PBMIntranasalConfig(
                    wavelength: p.wavelength.rawValue,
                    irradianceMWcm2: p.irradianceMWcm2,
                    frequencyHz: p.frequencyHz,
                    dutyCyclePercent: p.dutyCyclePercent,
                    durationSeconds: durationSeconds
                )))
            case .eegNeurofeedback(let p):
                modalities.append(.eegNeurofeedback(EEGConfig(
                    enabledChannels: p.resolvedChannels,
                    sampleRateHz: 500,
                    neurofeedbackBand: p.band.rawValue,
                    closedLoopEnabled: p.closedLoopEnabled
                )))
            case .besTacs(let p):
                // The T1 wire type is BESConfig regardless of waveform — tACS (sinusoidal)
                // and BES (asymmetric) differ only in waveform.rawValue on the wire.
                modalities.append(.bes(BESConfig(
                    frequencyHz: p.frequencyHz,
                    amplitudeMilliamps: p.intensityMilliamps,
                    durationSeconds: mod.interval.isContinuous ? durationSeconds : mod.interval.intervalOnSeconds,
                    waveform: p.waveform.rawValue
                )))
            case .tdcs(let p):
                modalities.append(.tdcs(TDCSConfig(
                    amplitudeMilliamps: p.intensityMilliamps,
                    durationSeconds: mod.interval.isContinuous ? durationSeconds : mod.interval.intervalOnSeconds,
                    rampSeconds: p.rampSeconds,
                    electrodePairs: p.electrodePairs,
                    electrodeAreaCm2: p.electrodeAreaCm2
                )))
            case .vnsHRV(let p):
                modalities.append(.vnsHRV(VNSHRVConfig(
                    frequencyHz: p.frequencyHz,
                    amplitudeMilliamps: p.intensityMilliamps,
                    enableHRVBiofeedback: true,
                    resonanceBreathingRateDefault: p.resonanceBreathingRate,
                    hrvProtocol: p.hrvProtocol.wireValue
                )))
            case .audioEntrainment(let p):
                modalities.append(.neuralAudio(NeuralAudioConfig(
                    binauralBeatHz: p.binauralBeatsHz,
                    isochronicToneHz: p.isochronicTonesHz,
                    noiseType: p.noiseType?.rawValue,
                    volumeDb: p.volumeDb,
                    eegAdaptive: p.eegAdaptive,
                    useBoneConductionForPacer: p.boneConductionPacer
                )))
            case .visualStimulation(let p):
                modalities.append(.visualStimulation(VisualStimConfig(
                    frequencyHz: p.frequencyHz,
                    mode: p.mode.sessionWireName,
                    enableModeFInvisibleNIR: p.enableModeF,
                    emdrCadenceHz: p.emdrCadenceHz
                )))
            default:
                // T2 modalities (21-ch qEEG, TMS, clinical tACS, HD-tDCS, 1170nm deep PBM,
                // cervical VNS) and accessory modalities (vibrotactile pad, Watch sync)
                // are not part of the T1 hub wire format. They require a T2 hub session.
                // NOTE: if a new T1 modality is added to NPModalityParams, add it above —
                // this default will silently drop it.
                break
            }
        }

        self.init(
            name: definition.name,
            modalities: modalities,
            totalDurationSeconds: durationSeconds,
            mode: mode
        )
    }
}

// MARK: - HRV protocol wire mapping

private extension NPVNSHRVParams.HRVProtocol {
    var wireValue: VNSHRVConfig.HRVProtocol {
        switch self {
        case .standalone:     return .standalone
        case .tavnsSync:      return .tavnsSynchronized
        case .eegBiofeedback: return .dualEEGBiofeedback
        case .combinedPBM:    return .combinedPBM
        }
    }
}

// MARK: - Refusals (NP-NPPS-REF-001 §4.1a, §5)
//
// Compiler diagnostics, English like hubCompiler.ts's on the web.

/// Why a PBM block's wavelength cannot be delivered. Thrown by the session compiler.
struct NPWavelengthRefusal: Error, LocalizedError, Equatable {
    let value: String
    let reason: NPWavelengthRules.Refusal

    var errorDescription: String? {
        switch reason {
        case .retired:
            return NPWavelengthRules.retiredMessage(value)
        case .invalid:
            return "PBM wavelength '\(value)' is not a wavelength: write one value such as \"810nm\"."
        case .unmapped:
            return "No emitter channel delivers \(value) under the wavelength rules in force. " +
                "Refused, not moved to the nearest channel."
        }
    }
}

/// A block the session wire cannot express. Refused rather than flattened.
struct NPUnsupportedTimingError: Error, LocalizedError, Equatable {
    var errorDescription: String? {
        "This protocol times a block with `start`, which the session wire cannot express yet. " +
            "Refused, not reshaped."
    }
}

// Compiler diagnostics are English, like hubCompiler.ts's.
extension NPWavelengthRules {
    /// The refusal text for a retired name, naming the blocks that replace it.
    static func retiredMessage(_ value: String) -> String {
        let blocks = (retired[value] ?? []).map { "\"\($0)\"" }.joined(separator: " and ")
        return "wavelength \"\(value)\" is retired: it welded independent emitters into one block. "
            + "Write one block per wavelength (\(blocks)), each with its own irradiance."
    }
}
