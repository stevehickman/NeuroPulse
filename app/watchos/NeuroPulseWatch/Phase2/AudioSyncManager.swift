import AVFoundation
import Combine

// Phase 2 — Audio sync to AirPods.
// Generates binaural beats and isochronic tones via AVAudioEngine with two
// independently-tuned sine oscillators (left/right channels).  Breathing pacer
// tones are also produced here, synchronized to PacerPhase events from the hub.
//
// Sync strategy: epoch is excluded from the WC bridge (UHDR boundary,
// NP-PRIV-ANALYSIS-002 MEDIUM-08) so sessionEpochMs is always 0 on Watch.
// Playback starts immediately with no meaningful sync delay.
// Sub-50ms sync requires a future non-UHDR clock mechanism.

final class AudioSyncManager: ObservableObject {

    @Published private(set) var isPlaying = false

    // MARK: - Public configuration

    struct Config {
        var binauralCarrierHz: Double = 200.0       // base carrier frequency
        var beatFrequencyHz:   Double = 10.0        // difference = beat frequency
        var pacerToneHz:       Double = 440.0       // pacer transition tone
        var volume:            Float  = 0.7
        var pacerToneEnabled:  Bool   = true
    }

    var config = Config()

    // MARK: - Private audio graph

    private let engine       = AVAudioEngine()
    private let mixerNode    = AVAudioMixerNode()

    private var pacerTimer:  Timer? = nil

    private let sampleRate: Double = 44100

    private var cancellables = Set<AnyCancellable>()

    // MARK: - Lifecycle

    init() {
        configureAudioSession()
        buildGraph()
    }

    func start(sessionEpochMs: UInt32) {
        guard !isPlaying else { return }
        let offsetMs = Double(UInt32(Date().timeIntervalSince1970 * 1000) &- sessionEpochMs)
        // Pre-buffer 200 ms to absorb WatchConnectivity latency jitter.
        let preBufferMs = max(0, 200.0 - offsetMs)
        DispatchQueue.main.asyncAfter(deadline: .now() + preBufferMs / 1000) { [weak self] in
            self?.startEngine()
        }
    }

    func stop() {
        pacerTimer?.invalidate()
        engine.stop()
        isPlaying = false
    }

    // Call on every PacerPhase update from the hub (via WatchSessionManager).
    func onPacerPhaseChanged(_ phase: PacerPhase) {
        guard config.pacerToneEnabled, isPlaying else { return }
        triggerPacerTone()
    }

    // MARK: - Private

    private func configureAudioSession() {
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .default, options: [.allowBluetooth, .allowBluetoothA2DP])
        try? session.setActive(true)
    }

    private func buildGraph() {
        let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2)!

        let leftNode = makeOscillatorNode(frequencyProvider: { [weak self] in
            guard let s = self else { return 0 }
            return s.config.binauralCarrierHz
        })
        let rightNode = makeOscillatorNode(frequencyProvider: { [weak self] in
            guard let s = self else { return 0 }
            return s.config.binauralCarrierHz + s.config.beatFrequencyHz
        })
        let pacerNode = makePacerNode()

        engine.attach(leftNode)
        engine.attach(rightNode)
        engine.attach(pacerNode)
        engine.attach(mixerNode)

        // Route left osc → L channel, right osc → R channel of stereo mixer.
        engine.connect(leftNode,  to: mixerNode, format: AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1)!)
        engine.connect(rightNode, to: mixerNode, format: AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1)!)
        engine.connect(pacerNode, to: mixerNode, format: AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1)!)
        engine.connect(mixerNode, to: engine.mainMixerNode, format: format)

        mixerNode.outputVolume = config.volume
    }

    private func makeOscillatorNode(frequencyProvider: @escaping () -> Double) -> AVAudioSourceNode {
        let sr = sampleRate
        var ph = 0.0
        return AVAudioSourceNode { _, _, frameCount, audioBufferList -> OSStatus in
            let ablPointer = UnsafeMutableAudioBufferListPointer(audioBufferList)
            let freq = frequencyProvider()
            let phaseIncrement = 2.0 * .pi * freq / sr
            for frame in 0..<Int(frameCount) {
                let sample = Float(sin(ph))
                ph += phaseIncrement
                if ph > 2.0 * .pi { ph -= 2.0 * .pi }
                for buffer in ablPointer {
                    let buf = buffer.mData!.assumingMemoryBound(to: Float.self)
                    buf[frame] = sample
                }
            }
            return noErr
        }
    }

    private var pacerTonePhase: Double = 0
    private var pacerSamplesToPlay: Int = 0
    private static let pacerToneDurationMs: Double = 80

    private func makePacerNode() -> AVAudioSourceNode {
        let sr = sampleRate
        let toneHz = config.pacerToneHz
        return AVAudioSourceNode { [weak self] _, _, frameCount, audioBufferList -> OSStatus in
            guard let self else { return noErr }
            let ablPointer = UnsafeMutableAudioBufferListPointer(audioBufferList)
            let phaseIncrement = 2.0 * .pi * toneHz / sr
            for frame in 0..<Int(frameCount) {
                var sample: Float = 0
                if self.pacerSamplesToPlay > 0 {
                    sample = Float(sin(self.pacerTonePhase)) * 0.3   // pacer tone at -10dB vs carrier
                    self.pacerTonePhase += phaseIncrement
                    if self.pacerTonePhase > 2.0 * .pi { self.pacerTonePhase -= 2.0 * .pi }
                    self.pacerSamplesToPlay -= 1
                }
                for buffer in ablPointer {
                    let buf = buffer.mData!.assumingMemoryBound(to: Float.self)
                    buf[frame] = sample
                }
            }
            return noErr
        }
    }

    private func triggerPacerTone() {
        // Arm the pacer render block to output a short tone.
        let samples = Int(sampleRate * Self.pacerToneDurationMs / 1000)
        pacerSamplesToPlay = samples
        pacerTonePhase = 0
    }

    private func startEngine() {
        do {
            try engine.start()
            isPlaying = true
        } catch {
            isPlaying = false
        }
    }
}

// MARK: - Beat preset factory

extension AudioSyncManager {
    enum BeatPreset {
        case gammaClarityBinaural       // 40Hz beat at 200Hz carrier
        case alphaCalm                  // 10Hz beat at 200Hz carrier
        case thetaMemory                // 6Hz beat at 200Hz carrier
        case sleepDelta                 // 2Hz beat at 200Hz carrier
        case focusPrime                 // 20Hz beat at 200Hz carrier
        case isochronicGamma            // 40Hz isochronic (amplitude modulation — single carrier)
    }

    func applyPreset(_ preset: BeatPreset) {
        switch preset {
        case .gammaClarityBinaural: config.binauralCarrierHz = 200; config.beatFrequencyHz = 40
        case .alphaCalm:            config.binauralCarrierHz = 200; config.beatFrequencyHz = 10
        case .thetaMemory:          config.binauralCarrierHz = 200; config.beatFrequencyHz = 6
        case .sleepDelta:           config.binauralCarrierHz = 200; config.beatFrequencyHz = 2
        case .focusPrime:           config.binauralCarrierHz = 200; config.beatFrequencyHz = 20
        case .isochronicGamma:      config.binauralCarrierHz = 200; config.beatFrequencyHz = 40
        }
    }
}
