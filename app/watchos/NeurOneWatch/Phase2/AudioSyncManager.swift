import AVFoundation
import Combine

// Phase 2 — Audio sync to AirPods.
// Generates binaural beats and isochronic tones via AVAudioEngine.
//   - Binaural:   two sine oscillators offset by the beat frequency, one per
//                 ear.  The "beat" exists only as the interaural difference.
//   - Isochronic: a single carrier (identical in both ears) whose amplitude is
//                 gated on/off at the entrainment frequency.  The pulsing tone
//                 itself is the entrainment stimulus — no interaural offset.
// Breathing pacer tones are also produced here, synchronized to PacerPhase
// events from the hub.
//
// Sync strategy: epoch is excluded from the WC bridge (UHDR boundary,
// NP-PRIV-ANALYSIS-002 MEDIUM-08) so sessionEpochMs is always 0 on Watch.
// Playback starts immediately with no meaningful sync delay.
// Sub-50ms sync requires a future non-UHDR clock mechanism.

final class AudioSyncManager: ObservableObject {

    @Published private(set) var isPlaying = false

    // MARK: - Public configuration

    enum EntrainmentMode {
        case binaural      // two carriers, L/R offset by beatFrequencyHz
        case isochronic    // single carrier both ears, amplitude gated at beatFrequencyHz
    }

    struct Config {
        var binauralCarrierHz: Double = 200.0       // base carrier frequency
        var beatFrequencyHz:   Double = 10.0        // binaural: interaural difference; isochronic: gate rate
        var pacerToneHz:       Double = 440.0       // pacer transition tone
        var volume:            Float  = 0.7
        var pacerToneEnabled:  Bool   = true
        var entrainmentMode:   EntrainmentMode = .binaural
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
        // Playback-only tone graph → A2DP (high-quality stereo) is the correct
        // Bluetooth route. The HFP option (.allowBluetooth) is telephony-grade,
        // unnecessary for playback, and is watchOS 11+ only.
        try? session.setCategory(.playback, mode: .default, options: [.allowBluetoothA2DP])
        try? session.setActive(true)
    }

    private func buildGraph() {
        let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2)!

        // Left ear is always the bare carrier.  In binaural mode the right ear
        // is offset by the beat frequency; in isochronic mode both ears share
        // the same carrier (the gate applied inside the node is the stimulus).
        let leftNode = makeOscillatorNode(offsetProvider: { 0 })
        let rightNode = makeOscillatorNode(offsetProvider: { [weak self] in
            guard let s = self else { return 0 }
            return s.config.entrainmentMode == .isochronic ? 0 : s.config.beatFrequencyHz
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

    // `offsetProvider` returns the per-render frequency offset added to the
    // carrier (used for the binaural right-ear detune).  In isochronic mode the
    // carrier is left un-offset and its amplitude is gated at beatFrequencyHz.
    private func makeOscillatorNode(offsetProvider: @escaping () -> Double) -> AVAudioSourceNode {
        let sr = sampleRate
        var carrierPhase = 0.0
        var modPhase = 0.0
        return AVAudioSourceNode { [weak self] _, _, frameCount, audioBufferList -> OSStatus in
            let ablPointer = UnsafeMutableAudioBufferListPointer(audioBufferList)
            guard let self else {
                for buffer in ablPointer { memset(buffer.mData, 0, Int(buffer.mDataByteSize)) }
                return noErr
            }
            let freq = self.config.binauralCarrierHz + offsetProvider()
            let carrierIncrement = 2.0 * .pi * freq / sr
            let isochronic = self.config.entrainmentMode == .isochronic
            let modIncrement = 2.0 * .pi * self.config.beatFrequencyHz / sr
            for frame in 0..<Int(frameCount) {
                var sample = Float(sin(carrierPhase))
                carrierPhase += carrierIncrement
                if carrierPhase > 2.0 * .pi { carrierPhase -= 2.0 * .pi }
                if isochronic {
                    // Raised-cosine amplitude gate: (1 - cos)/2 ranges 0→1→0 once
                    // per modulation period, pulsing the carrier at beatFrequencyHz.
                    // The smooth edges avoid click artifacts a hard on/off gate
                    // would produce at the transitions.
                    let gate = Float((1.0 - cos(modPhase)) * 0.5)
                    sample *= gate
                    modPhase += modIncrement
                    if modPhase > 2.0 * .pi { modPhase -= 2.0 * .pi }
                }
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
    /// Entrainment frequency band. Orthogonal to `EntrainmentMode` — every band
    /// can be delivered as either a binaural beat or an isochronic tone.
    enum BeatPreset {
        case gammaClarity               // 40Hz
        case alphaCalm                  // 10Hz
        case thetaMemory                // 6Hz
        case sleepDelta                 // 2Hz
        case focusPrime                 // 20Hz

        /// Entrainment frequency (interaural difference for binaural,
        /// amplitude-gate rate for isochronic).
        var beatFrequencyHz: Double {
            switch self {
            case .gammaClarity: return 40
            case .alphaCalm:    return 10
            case .thetaMemory:  return 6
            case .sleepDelta:   return 2
            case .focusPrime:   return 20
            }
        }
    }

    /// Applies a frequency band in the requested entrainment mode. Mode defaults
    /// to `.binaural`; pass `.isochronic` for a single-carrier amplitude-gated
    /// tone (see the render block for the raised-cosine gate).
    func applyPreset(_ preset: BeatPreset, mode: EntrainmentMode = .binaural) {
        config.binauralCarrierHz = 200
        config.beatFrequencyHz = preset.beatFrequencyHz
        config.entrainmentMode = mode
    }
}
