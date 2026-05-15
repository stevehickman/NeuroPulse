import Foundation

// MARK: - Predefined Protocols (DEPRECATED)
//
// DEPRECATED: This file is superseded by NPBundledProtocols.swift + the .npps files in
// protocols/predefined/. NPProtocolLibrary now loads predefined protocols by parsing
// NPBundledProtocols.allContents at startup. This file is retained for reference only
// and is no longer used by NPProtocolLibrary.
//
// DO NOT add new protocols here. Add them as .npps files in protocols/predefined/ and
// update NPBundledProtocols.swift accordingly.
//
// This file will be removed in a future cleanup pass.

enum NPPredefinedProtocols {

    // MARK: - Single Protocols

    static let gammaClarity: NPProtocolDefinition = {
        let interval = NPIntervalConfig(intervalOnSeconds: 120, intervalOffSeconds: 60, repeatCount: nil)
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000001-0000-0000-0000-000000000000")!,
            name: "Gamma Clarity",
            description: "40Hz multi-modal gamma entrainment for focus and cognitive clarity. Combines PBM, EEG-adaptive neurofeedback, binocular flicker, and isochronic audio.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["focus", "gamma", "40Hz", "cognitive"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 75,
                        frequencyHz: 40,
                        dutyCyclePercent: 25
                    )),
                    interval: interval,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .gamma,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .visualStimulation(NPVisualStimParams(
                        frequencyHz: 40,
                        mode: .binocular,
                        emdrCadenceHz: 1.0,
                        enableModeF: false
                    )),
                    interval: NPIntervalConfig(intervalOnSeconds: 60, intervalOffSeconds: 30, repeatCount: nil),
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: nil,
                        isochronicTonesHz: 40,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 60,
                        eegAdaptive: true,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let alphaCalm: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000002-0000-0000-0000-000000000000")!,
            name: "Alpha Calm",
            description: "10Hz alpha entrainment for relaxation and stress relief. Combines PBM, alpha EEG neurofeedback, HRV biofeedback, and binaural beats.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["calm", "alpha", "relaxation", "stress"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 75,
                        frequencyHz: 10,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .alpha,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 25,
                        intensityMilliamps: 1.5,
                        hrvProtocol: .standalone,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: 10,
                        isochronicTonesHz: nil,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 60,
                        eegAdaptive: true,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let thetaMemory: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000003-0000-0000-0000-000000000000")!,
            name: "Theta Memory",
            description: "6Hz theta entrainment for memory consolidation and learning. 30-minute session with closed-loop EEG adaptation.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["memory", "theta", "learning", "consolidation"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(30 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 70,
                        frequencyHz: 6,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .theta,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: 6,
                        isochronicTonesHz: nil,
                        noiseType: .brown,
                        carrierHz: 440,
                        volumePercent: 55,
                        eegAdaptive: true,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 25,
                        intensityMilliamps: 1.5,
                        hrvProtocol: .standalone,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let sleepDeep: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000004-0000-0000-0000-000000000000")!,
            name: "Sleep Deep",
            description: "2Hz delta PBM on rear zones with theta neurofeedback. Prepares for deep restorative sleep. 45-minute session.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["sleep", "delta", "recovery", "relaxation"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(45 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .rear, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 60,
                        frequencyHz: 2,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .delta,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: nil,
                        isochronicTonesHz: nil,
                        noiseType: .brown,
                        carrierHz: 440,
                        volumePercent: 40,
                        eegAdaptive: false,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 6,
                        intensityMilliamps: 1.0,
                        hrvProtocol: .standalone,
                        resonanceBreathingRate: 5.0
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let gammaThetaCoupled: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000005-0000-0000-0000-000000000000")!,
            name: "Gamma + Theta Coupled",
            description: "Simultaneous 40Hz front-zone PBM and 6Hz rear-zone PBM with gamma-theta EEG coupling, binaural + isochronic audio, 40Hz visual entrainment, and BES.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["focus", "gamma", "theta", "coupled", "advanced"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                // Front zones at 40Hz gamma
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .front, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 75,
                        frequencyHz: 40,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                // Rear zones at 6Hz theta
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .rear, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 70,
                        frequencyHz: 6,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .gammaTheta,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: 40,
                        isochronicTonesHz: 6,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 60,
                        eegAdaptive: true,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .visualStimulation(NPVisualStimParams(
                        frequencyHz: 40,
                        mode: .binocular,
                        emdrCadenceHz: 1.0,
                        enableModeF: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .besTacs(NPBESTacsParams(
                        frequencyHz: 40,
                        intensityMilliamps: 0.8,
                        waveform: .sinusoidal
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let focusPrime: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000006-0000-0000-0000-000000000000")!,
            name: "Focus Prime",
            description: "20Hz beta/focus protocol combining PBM, closed-loop EEG, BES, and binaural audio. BES runs in 30-on/30-off intervals for habituation prevention.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["focus", "beta", "cognitive", "productivity"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 80,
                        frequencyHz: 20,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .beta,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .besTacs(NPBESTacsParams(
                        frequencyHz: 20,
                        intensityMilliamps: 0.8,
                        waveform: .sinusoidal
                    )),
                    interval: NPIntervalConfig(intervalOnSeconds: 30, intervalOffSeconds: 30, repeatCount: nil),
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: 20,
                        isochronicTonesHz: nil,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 60,
                        eegAdaptive: true,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .visualStimulation(NPVisualStimParams(
                        frequencyHz: 20,
                        mode: .binocular,
                        emdrCadenceHz: 1.0,
                        enableModeF: false
                    )),
                    interval: NPIntervalConfig(intervalOnSeconds: 5 * 60, intervalOffSeconds: 2 * 60, repeatCount: nil),
                    enabled: true
                )
            ]
        )
    }()

    static let vascularBaseline: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000007-0000-0000-0000-000000000000")!,
            name: "Vascular Baseline",
            description: "CW (continuous wave) PBM for cerebrovascular circulation support. Low-frequency VNS and alpha EEG. Use as a maintenance or recovery session.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["circulation", "CW", "baseline", "maintenance"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 80,
                        frequencyHz: 0,   // CW
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 1,
                        intensityMilliamps: 1.0,
                        hrvProtocol: .standalone,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .alpha,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let hrvCoherenceTraining: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000008-0000-0000-0000-000000000000")!,
            name: "HRV Coherence Training",
            description: "Standalone HRV coherence training with resonance breathing pacer. Bone conduction delivers the breathing cue. Evidence: meta-analysis 24 RCTs (d=0.83 anxiety reduction).",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["HRV", "coherence", "stress", "relaxation", "breathing"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 25,
                        intensityMilliamps: 1.5,
                        hrvProtocol: .standalone,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: nil,
                        isochronicTonesHz: nil,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 40,
                        eegAdaptive: false,
                        boneConductionPacer: true
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let hrvTavnsSynchronised: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "00000009-0000-0000-0000-000000000000")!,
            name: "HRV + taVNS Synchronised",
            description: "Stimulation pulses timed to inspiration phase for optimised noradrenergic modulation. Alpha EEG neurofeedback adds cortical awareness layer.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["HRV", "VNS", "advanced", "synchronised"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 25,
                        intensityMilliamps: 1.5,
                        hrvProtocol: .tavnsSync,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .alpha,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let emdrSession: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "0000000A-0000-0000-0000-000000000000")!,
            name: "EMDR Session",
            description: "Eye Movement Desensitisation and Reprocessing (EMDR) left/right alternation at 1Hz. Pink noise masking, alpha EEG monitoring. 45-minute session.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["EMDR", "trauma", "processing", "visual"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(45 * 60),
            modalities: [
                NPProtocolModality(
                    params: .visualStimulation(NPVisualStimParams(
                        frequencyHz: 1,
                        mode: .emdr,
                        emdrCadenceHz: 1.0,
                        enableModeF: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: nil,
                        isochronicTonesHz: nil,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 50,
                        eegAdaptive: false,
                        boneConductionPacer: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .alpha,
                        closedLoopEnabled: false
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )
    }()

    static let fullT1Immersive: NPProtocolDefinition = {
        return NPProtocolDefinition(
            id: UUID(uuidString: "0000000B-0000-0000-0000-000000000000")!,
            name: "Full T1 Immersive",
            description: "All 8 T1 modalities running simultaneously with balanced settings. For experienced users. Intranasal PBM and tDCS run in time-limited intervals within the session.",
            author: "NeuroPulse",
            version: "1.0",
            tags: ["comprehensive", "all-modalities", "advanced", "T1"],
            createdAt: Date(timeIntervalSince1970: 0),
            modifiedAt: Date(timeIntervalSince1970: 0),
            isPredefined: true,
            timingMode: .duration(30 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        zones: .all, customZones: nil,
                        wavelength: .base660_808nm,
                        intensityPercent: 75,
                        frequencyHz: 40,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .pbmIntranasal(NPPBMIntranasalParams(
                        intensityPercent: 60,
                        frequencyHz: 40,
                        dutyCyclePercent: 25
                    )),
                    interval: NPIntervalConfig(intervalOnSeconds: 15 * 60, intervalOffSeconds: 15 * 60, repeatCount: 1),
                    enabled: true
                ),
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all, customChannels: nil,
                        band: .gamma,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .besTacs(NPBESTacsParams(
                        frequencyHz: 40,
                        intensityMilliamps: 0.8,
                        waveform: .sinusoidal
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .vnsHRV(NPVNSHRVParams(
                        frequencyHz: 25,
                        intensityMilliamps: 1.5,
                        hrvProtocol: .tavnsSync,
                        resonanceBreathingRate: 6.0
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .audioEntrainment(NPAudioEntrainmentParams(
                        binauralBeatsHz: nil,
                        isochronicTonesHz: 40,
                        noiseType: .pink,
                        carrierHz: 440,
                        volumePercent: 60,
                        eegAdaptive: true,
                        boneConductionPacer: true
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .visualStimulation(NPVisualStimParams(
                        frequencyHz: 40,
                        mode: .binocular,
                        emdrCadenceHz: 1.0,
                        enableModeF: false
                    )),
                    interval: .continuous,
                    enabled: true
                ),
                NPProtocolModality(
                    params: .tdcs(NPTDCSParams(
                        intensityMilliamps: 1.0,
                        electrodePairs: [["Fp1","P3"]],
                        rampSeconds: 30
                    )),
                    interval: NPIntervalConfig(intervalOnSeconds: 20 * 60, intervalOffSeconds: 10 * 60, repeatCount: 1),
                    enabled: true
                )
            ]
        )
    }()

    // MARK: - Composite Protocols

    static let calmToFocus: NPCompositeProtocol = NPCompositeProtocol(
        id: UUID(uuidString: "00000100-0000-0000-0000-000000000000")!,
        name: "Calm to Focus",
        description: "Transitions from alpha calm to focused beta. Layers overlap for 2 minutes, allowing smooth frequency transition.",
        author: "NeuroPulse",
        version: "1.0",
        tags: ["transition", "ramp", "productivity", "calm", "focus"],
        createdAt: Date(timeIntervalSince1970: 0),
        modifiedAt: Date(timeIntervalSince1970: 0),
        isPredefined: true,
        layers: [
            NPCompositeLayer(
                protocolName: "Alpha Calm",
                startOffsetSeconds: 0,
                durationSeconds: 10 * 60,
                intensityScale: 1.0
            ),
            NPCompositeLayer(
                protocolName: "Focus Prime",
                startOffsetSeconds: 8 * 60,
                durationSeconds: 20 * 60,
                intensityScale: 1.0
            )
        ],
        conflictResolution: .merge
    )

    static let sleepWindDown: NPCompositeProtocol = NPCompositeProtocol(
        id: UUID(uuidString: "00000101-0000-0000-0000-000000000000")!,
        name: "Sleep Wind-Down",
        description: "Sequential transition from alpha calm to deep sleep preparation. No overlap — alpha calm finishes before sleep deep begins.",
        author: "NeuroPulse",
        version: "1.0",
        tags: ["sleep", "wind-down", "transition", "evening"],
        createdAt: Date(timeIntervalSince1970: 0),
        modifiedAt: Date(timeIntervalSince1970: 0),
        isPredefined: true,
        layers: [
            NPCompositeLayer(
                protocolName: "Alpha Calm",
                startOffsetSeconds: 0,
                durationSeconds: 10 * 60,
                intensityScale: 1.0
            ),
            NPCompositeLayer(
                protocolName: "Sleep Deep",
                startOffsetSeconds: 10 * 60,
                durationSeconds: nil,   // full duration
                intensityScale: 1.0
            )
        ],
        conflictResolution: .sequential
    )

    // MARK: - All entries

    static var allSingle: [NPProtocolDefinition] {
        [
            gammaClarity,
            alphaCalm,
            thetaMemory,
            sleepDeep,
            gammaThetaCoupled,
            focusPrime,
            vascularBaseline,
            hrvCoherenceTraining,
            hrvTavnsSynchronised,
            emdrSession,
            fullT1Immersive
        ]
    }

    static var allComposite: [NPCompositeProtocol] {
        [calmToFocus, sleepWindDown]
    }

    static var all: [NPProtocolEntry] {
        allSingle.map { .single($0) } + allComposite.map { .composite($0) }
    }
}
