import CryptoKit
import Foundation

// UPLOAD_CHECK (SWR-UP-01, OI-UI-KMP-10 U1): what the helmet says it received. After every protocol upload the helmet
// hashes the buffer it received, before parsing, and reports that hash with an accept or refuse status. The app compares
// the hash with the SHA-256 of the exact bytes it sent. Wire layout: firmware/hub_control/include/np_upload_check.h,
// 34 bytes: status, reason, SHA-256. The hash is of protocol bytes the person authored and carries no user biology.

enum UploadCheckStatus: UInt8 {
    /// Nothing received since power-on.
    case none = 0
    case accepted = 1
    case refused = 2
    /// Hashed, verdict not yet reached: the app reads again, and never sends the protocol again (SWR-UP-03).
    case pending = 3
}

struct UploadCheck: Equatable {
    var status: UploadCheckStatus
    /// The refusal's `np_hub_status_t` code as a positive number; 0 unless `status` is `.refused`.
    var reason: Int
    /// SHA-256 of the received bytes, 64 lowercase hex digits; all zeros while `status` is `.none`.
    var receivedHash: String

    static let length = 34

    /// `NP_HUB_ERR_HW_VERSION_UNVERIFIED`: the helmet had not yet heard its own hardware version from the safety MCU, so it
    /// checked nothing (SWR-HV-08). Unlike the mismatch (27), sending the same protocol again can succeed.
    static let reasonHardwareVersionUnverified = 28

    /// `NP_HUB_ERR_TIER_UNVERIFIED`: the hub had not yet heard its tier from the safety MCU, so it checked nothing.
    static let reasonTierUnverified = 20

    /// Nil for a frame of the wrong length or an unknown status: no part of it is believed.
    init?(wire data: Data) {
        guard data.count == Self.length, let status = UploadCheckStatus(rawValue: data[data.startIndex]) else { return nil }
        self.status = status
        self.reason = Int(data[data.startIndex + 1])
        self.receivedHash = data.dropFirst(2).map { String(format: "%02x", $0) }.joined()
    }

    init(status: UploadCheckStatus, reason: Int, receivedHash: String) {
        self.status = status
        self.reason = reason
        self.receivedHash = receivedHash
    }

    /// The hash of the exact bytes the app sent, in the form `receivedHash` takes.
    static func hash(of sent: Data) -> String {
        SHA256.hash(data: sent).map { String(format: "%02x", $0) }.joined()
    }
}

/// What the library's upload rule is told (`SWR-UP-02`). Whether it was confirmed, unknown or failed is the core's decision
/// (`NPProtocolLibrary.recordUploadResult`); this only reads the helmet's answer.
struct UploadVerdict: Equatable {
    var helmetStatus: String?
    var helmetHash: String?
    var helmetRefusal: String?
    /// The helmet's refusal reason (`np_hub_status_t` as a positive number) when it refused, else nil. The library needs only the
    /// kind (`helmetRefusal`); a screen needs the reason to say whether to try again (`helmetNotReady`) or not.
    var helmetReason: Int?

    /// True when the hub refused only because it had not yet heard its own tier or hardware version from the safety MCU
    /// (`NP_HUB_ERR_TIER_UNVERIFIED`, `NP_HUB_ERR_HW_VERSION_UNVERIFIED`): it checked nothing, and sending the same protocol
    /// again can succeed. The other refusals will not change on a second try.
    var helmetNotReady: Bool {
        helmetReason == UploadCheck.reasonTierUnverified || helmetReason == UploadCheck.reasonHardwareVersionUnverified
    }

    /// The hub refused for its tier (`NP_HUB_ERR_TIER_F4` or `_UNVERIFIED`).
    var refusedForTier: Bool { helmetRefusal == UploadCheck.refusalTier }

    /// The hub refused for its hardware version (`NP_HUB_ERR_HW_VERSION_MISMATCH` or `_UNVERIFIED`).
    var refusedForHardwareVersion: Bool { helmetRefusal == UploadCheck.refusalHardwareVersion }
}
