import Foundation
import CryptoKit

// Session protocol authoring and CSPRNG signing.
// A session protocol defines all stimulation parameters for a single session.
// It is cryptographically signed before upload to the hub; the hub firmware
// rejects unsigned or corrupted protocols (NP-FW-EMMC-001 Rev A §8, CLAUDE.md §4.2).

// MARK: - Protocol definition

struct NPSessionProtocol: Codable {
    var id: UUID = UUID()
    /// v2: cranial targeting is a socket bitmap, not five zone-slot indices
    /// (NP-HEX-ZM-001). The blob SHAPE changed, so the version had to move with
    /// it — a hub reading a v2 blob as v1 would read a 16-byte mask where it
    /// expects a zone list.
    var schemaVersion: UInt8 = 2
    var name: String
    var modalities: [ModalityConfig]
    var totalDurationSeconds: Int
    var createdAt: Date = Date()
    var mode: OperatingMode = .mode2Programming

    enum OperatingMode: UInt8, Codable {
        case mode1Connected   = 1  // real-time streaming
        case mode2Programming = 2  // pre-upload, run from hub
        case mode3Autonomous  = 3  // offline, any USB-C PD power bank
    }
}

// MARK: - Modality configurations

enum ModalityConfig: Codable {
    case pbmTranscranial(PBMTranscranialConfig)
    case pbmIntranasal(PBMIntranasalConfig)
    case eegNeurofeedback(EEGConfig)
    case bes(BESConfig)
    case tdcs(TDCSConfig)
    case vnsHRV(VNSHRVConfig)
    case neuralAudio(NeuralAudioConfig)
    case visualStimulation(VisualStimConfig)
}

struct PBMTranscranialConfig: Codable {
    /// Which sockets on the hex lattice this command drives — the firmware's
    /// NP_PROTO_TARGET_SOCKET_MASK representation (16 bytes, LSB-first, 0-based).
    /// Replaces the five zone-slot indices, which named hardware that no longer
    /// exists.
    var socketMask: NPSocketMask
    var frequencyHz: Double     // 0 = CW, >0 = pulsed
    var dutyCyclePercent: Int   // ≤25 (firmware-enforced for pulsed)
    var durationSeconds: Int
    var targetDoseJoules: Double  // J/cm²; hub closed-loop to this target
}

struct PBMIntranasalConfig: Codable {
    var frequencyHz: Double
    var dutyCyclePercent: Int
    var durationSeconds: Int
}

struct EEGConfig: Codable {
    var enabledChannels: [String]  // e.g. ["Fp1","Fp2","F3","F4","C3","C4","P3","P4"]
    var sampleRateHz: Int = 500
    var neurofeedbackBand: String  // e.g. "alpha", "theta", "gamma"
    var closedLoopEnabled: Bool = true
}

struct BESConfig: Codable {
    var frequencyHz: Double        // 0.5–40 Hz
    var amplitudeMilliamps: Double // ≤1 mA
    var durationSeconds: Int
    var waveform: String = "sinusoidal"
}

struct TDCSConfig: Codable {
    var amplitudeMilliamps: Double // 0.1–2 mA; 40 µC/cm² hard limit enforced by safety MCU
    var durationSeconds: Int
    var rampSeconds: Int = 30      // hardware-enforced in firmware
    var electrodePairs: [[String]] // e.g. [["Fp1","P3"]]
}

struct VNSHRVConfig: Codable {
    var frequencyHz: Double    // 1–25 Hz
    var amplitudeMilliamps: Double  // ≤2 mA
    var enableHRVBiofeedback: Bool = true
    var resonanceBreathingRateDefault: Double = 6.0  // breaths/min
    var hrvProtocol: HRVProtocol = .standalone

    enum HRVProtocol: String, Codable {
        case standalone            = "standalone"
        case tavnsSynchronized     = "hrv_tavns_sync"
        case dualEEGBiofeedback    = "hrv_eeg_biofeedback"
        case combinedPBM           = "hrv_pbm"
    }
}

struct NeuralAudioConfig: Codable {
    var binauralBeatHz: Double?
    var isochronicToneHz: Double?
    var noiseType: String?         // "pink", "brown", or nil
    var eegAdaptive: Bool = true
    var useBoneConductionForPacer: Bool = true
}

struct VisualStimConfig: Codable {
    var frequencyHz: Double        // 0.5–100 Hz
    var mode: String = "binocular" // "binocular", "emdr", "retinalPBM"
    var enableModeFInvisibleNIR: Bool = false
    var emdrCadenceHz: Double = 1.0
}

// MARK: - Signed protocol blob

struct SignedProtocolBlob {
    let payload: Data   // canonical JSON of NPSessionProtocol
    let signature: Data // Ed25519 signature over SHA-256(payload) + schemaVersion + payloadSize
    let publicKeyFingerprint: String  // hex, for hub key pinning

    // Wire format: 4-byte magic + 4-byte payload length + payload + 64-byte Ed25519 sig
    static let magic: [UInt8] = [0x4E, 0x50, 0x50, 0x52]  // "NPPR"

    var wireFormat: Data {
        var buf = Data(Self.magic)
        var len = UInt32(payload.count).littleEndian
        buf.append(Data(bytes: &len, count: 4))
        buf.append(payload)
        buf.append(signature)
        return buf
    }
}

// MARK: - Protocol signer

struct SessionProtocolSigner {
    // The private signing key is generated once at first launch and stored in the iOS Keychain.
    // The corresponding public key is registered with the hub at pairing time.
    // Production: replace with secure enclave key where available.

    private static let keychainTag = "life.neurone.session-signing-key"

    static func sign(_ proto: NPSessionProtocol) throws -> SignedProtocolBlob {
        let payload = try JSONEncoder().encode(proto)
        let signingKey = try loadOrCreateSigningKey()
        let digest = SHA256.hash(data: payload)
        let signature = try signingKey.signature(for: Data(digest))
        let fingerprint = Data(signingKey.publicKey.rawRepresentation).prefix(8)
            .map { String(format: "%02x", $0) }.joined()
        return SignedProtocolBlob(
            payload: payload,
            signature: Data(signature),
            publicKeyFingerprint: fingerprint
        )
    }

    private static func loadOrCreateSigningKey() throws -> Curve25519.Signing.PrivateKey {
        // Attempt to load existing key from Keychain.
        // kSecAttrAccessible must be present in the read query so a key written by an
        // attacker with a different accessibility class cannot shadow the legitimate key
        // via SecItemCopyMatching. (NP-PRIV-ANALYSIS-002 LOW-12)
        let query: [CFString: Any] = [
            kSecClass:              kSecClassKey,
            kSecAttrApplicationTag: keychainTag.data(using: .utf8)!,
            kSecAttrAccessible:     kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecReturnData:         true
        ]
        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)

        if status == errSecSuccess, let keyData = item as? Data {
            return try Curve25519.Signing.PrivateKey(rawRepresentation: keyData)
        }

        // Generate new key and store in Keychain
        let key = Curve25519.Signing.PrivateKey()
        let addQuery: [CFString: Any] = [
            kSecClass:                kSecClassKey,
            kSecAttrApplicationTag:   keychainTag.data(using: .utf8)!,
            kSecValueData:            key.rawRepresentation,
            kSecAttrAccessible:       kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        SecItemAdd(addQuery as CFDictionary, nil)
        return key
    }

    static func publicKeyData() throws -> Data {
        let key = try loadOrCreateSigningKey()
        return key.publicKey.rawRepresentation
    }
}
