import Foundation

/// What the protocol screen does once a protocol has been sent: wait for the hub's report, record how the upload ended, and
/// say why the hub refused it (SWR-UP-01/02/03, SWR-HV-08, REQ-UPG-01). It holds no view state, so the screen's behaviour
/// is tested without a view: `say` is how a refusal reaches the screen.
@MainActor
struct ProtocolUploadFlow {
    let uploader: SessionProtocolUploader
    let library: NPProtocolLibrary

    /// Wait for the hub's report of the upload whose bytes hash to `sentHash` and record the result in the library. A
    /// refusal for the upload just sent is passed to `say` and returned, and so is a final answer for other bytes
    /// (`.unmatched`): nothing is known about this upload then. No answer at all is the unknown outcome and returns nil, as
    /// an accepted upload does (SWR-UP-03).
    @discardableResult
    func record(of entry: NPProtocolEntry, sentHash: String, say: (String) -> Void) async -> HelmetRefusalCause? {
        let verdict = await uploader.verify(sentHash: sentHash)
        library.recordUploadResult(entry, sentHash: sentHash, helmetStatus: verdict.helmetStatus,
                                   helmetHash: verdict.helmetHash, helmetRefusal: verdict.helmetRefusal)
        // A tier refusal never names a purchase (NP-PWRSRC-001 §6.3).
        let cause = verdict.cause(forSentHash: sentHash)
        if let cause { say(Self.refusalMessage(for: cause)) }
        return cause
    }

    /// Mode 3 programming. The screen closes and tells the person "Protocol stored" only if the hub holds the protocol, so
    /// this waits for its report. `send` writes the protocol and returns the SHA-256 of the bytes it sent. Returns true when
    /// the screen may close and report the protocol stored; false after any refusal, which has been passed to `say`.
    func program(entry: NPProtocolEntry, send: () async throws -> String, say: (String) -> Void) async throws -> Bool {
        let sentHash = try await send()
        return await record(of: entry, sentHash: sentHash, say: say) == nil
    }

    /// What to say for each reason the hub can refuse a protocol.
    static func refusalMessage(for cause: HelmetRefusalCause) -> String {
        switch cause {
        case .tier: return String(localized: "PROTOCOL_MENU_TIER_REFUSED")
        case .hardwareVersion: return String(localized: "PROTOCOL_MENU_HARDWARE_VERSION_REFUSED")
        case .notReady: return String(localized: "PROTOCOL_MENU_HELMET_NOT_READY")
        case .signature: return String(localized: "PROTOCOL_MENU_REFUSED_SIGNATURE")
        case .wrongDevice: return String(localized: "PROTOCOL_MENU_REFUSED_WRONG_DEVICE")
        case .formatVersion: return String(localized: "PROTOCOL_MENU_REFUSED_FORMAT_VERSION")
        case .damaged: return String(localized: "PROTOCOL_MENU_REFUSED_DAMAGED")
        case .malformed: return String(localized: "PROTOCOL_MENU_REFUSED_MALFORMED")
        case .sessionActive: return String(localized: "PROTOCOL_MENU_REFUSED_SESSION_ACTIVE")
        case .bootRefused: return String(localized: "PROTOCOL_MENU_REFUSED_BOOT")
        case .other: return String(localized: "PROTOCOL_MENU_REFUSED_GENERIC")
        case .unmatched: return String(localized: "PROTOCOL_MENU_UPLOAD_UNMATCHED")
        }
    }
}
