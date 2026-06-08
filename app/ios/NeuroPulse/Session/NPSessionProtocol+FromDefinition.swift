import Foundation

// Converts an NPProtocolDefinition (the rich app model) to the NPSessionProtocol
// wire format understood by hub firmware. Used by SessionProtocolUploader when
// the caller provides an NPProtocolDefinition rather than a pre-built NPSessionProtocol.

extension NPSessionProtocol {

    init(from definition: NPProtocolDefinition, mode: OperatingMode = .mode2Programming) {
        let durationSeconds = definition.totalDurationSeconds ?? 20 * 60
        var modalities: [ModalityConfig] = []

        for mod in definition.modalities where mod.enabled {
            switch mod.params {
            case .pbmTranscranial(let p):
                modalities.append(.pbmTranscranial(PBMTranscranialConfig(
                    zones: p.resolvedZones,
                    frequencyHz: p.frequencyHz,
                    dutyCyclePercent: p.dutyCyclePercent,
                    durationSeconds: durationSeconds,
                    targetDoseJoules: Double(durationSeconds) * (p.intensityPercent / 100.0) * 0.4
                )))
            case .pbmIntranasal(let p):
                modalities.append(.pbmIntranasal(PBMIntranasalConfig(
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
                    electrodePairs: p.electrodePairs
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
                    eegAdaptive: p.eegAdaptive,
                    useBoneConductionForPacer: p.boneConductionPacer
                )))
            case .visualStimulation(let p):
                modalities.append(.visualStimulation(VisualStimConfig(
                    frequencyHz: p.frequencyHz,
                    mode: p.mode.rawValue,
                    enableModeFInvisibleNIR: p.enableModeF,
                    emdrCadenceHz: p.emdrCadenceHz
                )))
            default:
                // T2 and accessory modalities not yet mapped to hub wire format.
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
        case .tavnsSync:      return .tavnsSynchronised
        case .eegBiofeedback: return .dualEEGBiofeedback
        case .combinedPBM:    return .combinedPBM
        }
    }
}
